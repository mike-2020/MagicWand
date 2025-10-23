/* Example of Voice Activity Detection (VAD)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "board.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "esp_vad.h"
#include "STTService.h"
#include "SpeechAudioCapture.h"
#include "ADFNotSupportCPP.h"

#include "audio_idf_version.h"

static const char *TAG = "EXAMPLE-VAD";
static vad_handle_t g_vadHandle;
static SpeechAudioCapture g_speachCapture;
static audio_element_handle_t i2s_stream_reader, filter, raw_read;
static audio_pipeline_handle_t pipeline;

#define VAD_SAMPLE_RATE_HZ  16000
#define VAD_FRAME_LENGTH_MS 30
#define VAD_BUFFER_LENGTH   (VAD_FRAME_LENGTH_MS * VAD_SAMPLE_RATE_HZ / 1000)
#define VAD_BUFFER_OFFSET   (10 * VAD_SAMPLE_RATE_HZ / 1000)


SpeechAudioCapture *SpeechAudioCapture::getInstance()
{
    return &g_speachCapture;
}

SpeechAudioCapture::SpeechAudioCapture()
{
    m_bStopFlag = true;
    m_bIsRunning = false;
    m_bPauseCaptureFlag = false;
}

int SpeechAudioCapture::start()
{
    //check pause capture flag
    while(1) {
        if(m_bPauseCaptureFlag==true) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }else{
            break;
        }
    }
    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[2.1] Create i2s stream to read audio data from codec chip");
    //i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, 16000, 16, AUDIO_STREAM_READER);
    i2s_stream_reader = build_i2s_stream(AUDIO_STREAM_READER);

    ESP_LOGI(TAG, "[2.2] Create filter to resample audio data");
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 16000;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = VAD_SAMPLE_RATE_HZ;
    rsp_cfg.dest_ch = 1;
    filter = rsp_filter_init(&rsp_cfg);

    ESP_LOGI(TAG, "[2.3] Create raw to receive data");
    raw_stream_cfg_t raw_cfg = {
        .type = AUDIO_STREAM_READER,
        .out_rb_size = 8 * 1024,
    };
    raw_read = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[ 3 ] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, filter, "filter");
    audio_pipeline_register(pipeline, raw_read, "raw");

    ESP_LOGI(TAG, "[ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[VAD]");
    const char *link_tag[3] = {"i2s", "filter", "raw"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 6 ] Initialize VAD handle");
    g_vadHandle = vad_create(VAD_MODE_2);

    m_bStopFlag = false;
    m_bPauseCaptureFlag = false;
    xTaskCreate(SpeechAudioCapture::proc, "speech_capture", 1024*6, (void*)this, 5, NULL);
    return 0;
}

void SpeechAudioCapture::proc(void *ctx)
{
    SpeechAudioCapture *handle = (SpeechAudioCapture*)ctx;
    int  rc = 0;
    char *pFrameBuffer = NULL;
    size_t len = 0;
    uint16_t seq = 0;
    uint16_t silenceTime = 0;
    uint16_t totalTime = 0;
    bool lastSpeechFlag = false;
    handle->m_bIsRunning = true;
    while (handle->m_bStopFlag!=true) {
        if(handle->m_bPauseCaptureFlag==true) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        if(pFrameBuffer==NULL) {
            rc = STTService::getInstance()->getFrameBuffer(&pFrameBuffer, &len);
            if(rc!=ESP_OK) {
                ESP_LOGE(TAG, "Failed to get frame buffer.");
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }else{
                ESP_LOGI(TAG, "Got frame buffer: ptr = %p, len = %d.", pFrameBuffer, len);
            }
        }

        raw_stream_read(raw_read, (char *)pFrameBuffer, len);

        // Feed samples to the VAD process and get the result
        vad_state_t vad_state = vad_process(g_vadHandle, (int16_t *)pFrameBuffer, VAD_SAMPLE_RATE_HZ, VAD_FRAME_LENGTH_MS);
        if(vad_state != VAD_SPEECH) {
            vad_state = vad_process(g_vadHandle, ((int16_t *)pFrameBuffer)+VAD_BUFFER_OFFSET, VAD_SAMPLE_RATE_HZ, VAD_FRAME_LENGTH_MS);
        }
        //speech detected, reset silence time counter
        if(vad_state == VAD_SPEECH) {
            ESP_LOGI(TAG, "Speech detected");
            silenceTime = 0;
        //if an ASR process has been started, but no speech detected in the current frame, increase silence time counter
        }else if(seq>0) {
            silenceTime += 40;
        }
        //either speech detected, or silence time does not exceed uplimit, we would send the audio frame for ASR
        if (vad_state == VAD_SPEECH || (seq > 0 && silenceTime <= 1000)) {
            ESP_LOGI(TAG, "Sending audio frame: vad = %d, seq = %u.", vad_state, seq);
            //STTService::getInstance()->sendFrame(pFrameBuffer, len, seq);
            seq++;
            totalTime += 40;
            pFrameBuffer = NULL;    //set it to NULL, so it will get new frame buffer at the start of next loop
            lastSpeechFlag = true;
        }

        if (vad_state != VAD_SPEECH) {
            lastSpeechFlag = false;
        }

        //either too long silence time, or exceed uplimit of seech time: send final signal
        if(seq>0 && silenceTime>=1000 || totalTime >= 60*1000) {
            ESP_LOGI(TAG, "Sending ending audio frame.");
            //STTService::getInstance()->sendFrame(NULL, 0, SEQ_LAST_NUM);
            seq = 0;
            totalTime = 0;
            silenceTime = 0;
            lastSpeechFlag = false;
        }
    }
    handle->m_bIsRunning = false;
    vTaskDelete(NULL);
}

int SpeechAudioCapture::stop()
{
    //stop the thread
    m_bStopFlag = true;
    while(m_bIsRunning != false) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "[ 7 ] Destroy VAD");
    vad_destroy(g_vadHandle);

    ESP_LOGI(TAG, "[ 8 ] Stop audio_pipeline and release all resources");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, filter);
    audio_pipeline_unregister(pipeline, raw_read);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(filter);
    audio_element_deinit(raw_read);
    return 0;
}
