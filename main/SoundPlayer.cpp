#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <string.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_task_wdt.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "filter_resample.h"
#include "esp_peripherals.h"
#include "esp_random.h"
#include "SoundPlayer.h"
#include "mp3_decoder.h"
#include "fatfs_stream.h"
#include "ASRService.h"
#include "AudioMgr.h"
#include "PinDef.h"
#include "PowerMgr.h"
#include "SpeechAudioCapture.h"

extern "C" audio_element_handle_t build_i2s_stream(audio_stream_type_t stream_type);

static const char *TAG = "Sound_Player";
static SoundPlayer g_SoundPlayer;

static audio_element_handle_t mp3_decoder         = NULL;
static audio_element_handle_t fatfs_stream_reader = NULL;
static audio_element_handle_t raw_writer          = NULL;
static audio_element_handle_t i2s_stream_writer   = NULL;
static audio_event_iface_handle_t pipeline_evt;


int SoundPlayer::mp3DecoderReadCB(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    SoundPlayer *handle = (SoundPlayer*)ctx;

    if(handle->m_curFileHandle!=NULL) {
        //ESP_LOGI(TAG, "Read from file buffer: len = %d, wait = %d.", len, wait_time);
        if(feof(handle->m_curFileHandle)) return AEL_IO_DONE;
        return fread(buf, sizeof(char), len, handle->m_curFileHandle);
    }

    if(handle->m_curRingBufHandle!=NULL) {
        int rc = rb_read(handle->m_curRingBufHandle, buf, len, wait_time);
        //ESP_LOGI(TAG, "Read from ring buffer: len = %d, wait = %d, rc = %d.", len, wait_time, rc);
        if(rc>=0) return rc;
        if(rc==RB_DONE) {
            rb_reset_is_done_write(handle->m_curRingBufHandle);
            ESP_LOGW(TAG, "return AEL_IO_DONE.");
            return AEL_IO_DONE;
        }else{
            ESP_LOGE(TAG, "return AEL_IO_FAIL.");
            return AEL_IO_FAIL;
        }
    }

    return AEL_IO_FAIL;
}

void setIdleState()
{
    i2s_stream_set_clk(i2s_stream_writer, 16000, 16, AUDIO_MIC_CHAN_NUM);    //looks like i2s_stream_writer and i2s_stream_reader share the same set of parameters. This is to reset the parameters required by ASR
    PowerMgr::getInstance()->disablePA();
    ASRService::getInstance()->enable();
    SpeechAudioCapture::getInstance()->resume();
}

void setBusyState()
{
    PowerMgr::getInstance()->enablePA();
    ASRService::getInstance()->disable();
    SpeechAudioCapture::getInstance()->pause();
}

void SoundPlayer::proc(void* ctx)
{
    SoundPlayer *handle = (SoundPlayer*)ctx;
    audio_pipeline_handle_t pipeline = (audio_pipeline_handle_t)handle->m_audioPipeline;

    ESP_LOGI(TAG, "Sound Player daemon task is running...");
    audio_element_state_t el_state = AEL_STATE_NONE;
    while (1) {
        if(el_state == AEL_STATE_NONE || el_state == AEL_STATE_FINISHED || el_state == AEL_STATE_ERROR) {
            setIdleState();
            if(handle->processNextCmd()!=ESP_OK) continue;
            setBusyState();
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
            audio_pipeline_reset_ringbuffer(pipeline);
            audio_pipeline_reset_elements(pipeline);
            audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
            audio_pipeline_run(pipeline);
            el_state = AEL_STATE_INIT;
        }

        /* Handle event interface messages from pipeline
           to set music info and to advance to the next song
        */
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(pipeline_evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        //ESP_LOGI(TAG, "msg.source_type = %d", msg.source_type);

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
            // Set music info for a new song to be played
            if (msg.source == (void *) mp3_decoder) {
                if(msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
                    audio_element_info_t music_info = {0};
                    audio_element_getinfo(mp3_decoder, &music_info);
                    ESP_LOGI(TAG, "[ * ] Received music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                            music_info.sample_rates, music_info.bits, music_info.channels);
                    audio_element_setinfo(i2s_stream_writer, &music_info);
                    i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
                }else if(msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                    el_state = audio_element_get_state(mp3_decoder);
                    ESP_LOGI(TAG, "MP3 Decoder state: %d.", el_state);
                }
                continue;
            }
            // Advance to the next song when previous finishes
            if (msg.source == (void *) i2s_stream_writer
                && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                el_state = audio_element_get_state(i2s_stream_writer);
                if (el_state == AEL_STATE_FINISHED) {
                    ESP_LOGI(TAG, "[ * ] Finished, waiting for the next song");
                }
                continue;
            }

            if(msg.source == (void*)fatfs_stream_reader
               && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                el_state = audio_element_get_state(fatfs_stream_reader);
                if (el_state == AEL_STATE_ERROR) {
                    ESP_LOGW(TAG, "fatfs operation error, reset state.");
                    //audio_pipeline_reset_elements(pipeline);
                }else{
                    el_state = AEL_STATE_INIT; //rewind state because it is just modified
                }
                continue;
            }
        }
    }

    ESP_LOGI(TAG, "[7.0] Stop audio_pipeline");
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    //audio_pipeline_unregister(pipeline, tts_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);

    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(pipeline_evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    //audio_element_deinit(tts_stream_reader);
    audio_element_deinit(i2s_stream_writer);

    vTaskDelete(NULL);
}

FILE * SoundPlayer::openNextFile()
{
    char name[64];
    char *q = m_nextFileNamePtr;
    while(*q != ':' && *q != '\0')q++;
    if(*q==':') {
        *q = '\0';
        sprintf(name, "/res/%s.mp3", m_nextFileNamePtr);
        m_nextFileNamePtr = q + 1;
    }else{ //*q == '\0', it means last element.
        sprintf(name, "/res/%s.mp3", m_nextFileNamePtr);
        m_nextFileNamePtr = NULL;
    }
    FILE *fp = fopen(name, "rb");
    if(fp==NULL) {
        ESP_LOGE(TAG, "Failed to open file %s: %s.", name, strerror(errno));
        return NULL;
    }
    ESP_LOGI(TAG, "Successfully opened file %s.", name);
    return fp;
}

int SoundPlayer::processNextCmd()
{
    if(m_curFileHandle!=NULL) {
        fclose(m_curFileHandle);
        m_curFileHandle = NULL;
    }

    if(m_curCmd.mode==SOUNDPLAYER_MODE_MP3FILE && m_nextFileNamePtr != NULL) {  //m_nextFileNamePtr will be set to NULL in function openNextFile()
        m_curFileHandle = openNextFile();
        if(m_curFileHandle!=NULL) return ESP_OK;
    }

    if(m_curRingBufHandle != NULL) {
        ESP_LOGW(TAG, "discard current ring buffer handle.");
        m_curRingBufHandle = NULL;
    }

    if(m_curCmd.task != NULL) xTaskNotifyGive(m_curCmd.task);

    while(1) {
        if (xQueueReceive(m_cmdQueue, &m_curCmd, pdMS_TO_TICKS(100))!=pdTRUE) continue;
        ESP_LOGW(TAG, "Received sound command, mode =%d.", m_curCmd.mode);
        break;
    }

    if(m_curCmd.mode==SOUNDPLAYER_MODE_MP3FILE) {
        m_nextFileNamePtr = m_curCmd.str;
        m_curFileHandle = openNextFile();
        if(m_curFileHandle!=NULL) return ESP_OK;
    }else if(m_curCmd.mode==SOUNDPLAYER_MODE_MP3TTS){
        ESP_LOGI(TAG, "Got ring buffer pipeline request.");
        m_curRingBufHandle = (ringbuf_handle_t)m_curCmd.ptr;
        return ESP_OK;
    }
    return ESP_FAIL;
}

SoundPlayer* SoundPlayer::getInstance()
{
    return &g_SoundPlayer;
}

SoundPlayer::SoundPlayer()
{
    m_cmdQueue = NULL;
    m_nVolume = 50;
    m_curFileHandle = NULL;
    m_nextFileNamePtr = NULL;
    m_curRingBufHandle = NULL;
    m_curCmd.mode = SOUNDPLAYER_MODE_NONE;
}

int SoundPlayer::init()
{
    PowerMgr::getInstance()->powerOn5V();

    m_cmdQueue = xQueueCreate(8, sizeof(sound_cmd_t));

    audio_pipeline_handle_t pipeline;

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);


    ESP_LOGI(TAG, "[3.2] Create i2s stream to write data to codec chip");
    i2s_stream_writer = build_i2s_stream(AUDIO_STREAM_WRITER);
    //i2s_alc_volume_set(i2s_stream_writer, m_nVolume);
/*
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[3.4] Create raw to write data");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_writer = raw_stream_init(&raw_cfg);
*/

    ESP_LOGI(TAG, "[3.3] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_cfg.stack_in_ext = false;
    mp3_decoder = mp3_decoder_init(&mp3_cfg);
    audio_element_set_read_cb(mp3_decoder, (stream_func)SoundPlayer::mp3DecoderReadCB, this);

    ESP_LOGI(TAG, "[3.5] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    //audio_pipeline_register(pipeline, fatfs_stream_reader, "fat");
    //audio_pipeline_register(pipeline, raw_writer, "raw");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[3.6] Link it together: fatfs-->mp3-->i2s_stream-->[codec_chip]");
    const char *link_tag[2] = {"mp3", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "[4.0] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    pipeline_evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, pipeline_evt);

    ESP_LOGI(TAG, "[6.0] Listen for all pipeline events");

    m_audioPipeline = pipeline;
    
    xTaskCreate(SoundPlayer::proc, "sound_player", 1024*4, (void*)this, 5, NULL);
    return 0;
}

ringbuf_handle_t SoundPlayer::openRingBuffer(int block_size, int n_blocks)
{
    sound_cmd_t cmd;
    ringbuf_handle_t handle = rb_create(block_size, n_blocks);
    memset(&cmd, 0, sizeof(cmd));
    cmd.ptr = (void *)handle;
    cmd.mode = SOUNDPLAYER_MODE_MP3TTS;
    if(xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE)
    {
        rb_destroy(handle);
        return NULL;
    }
    return handle;
}

int SoundPlayer::sendRingBufferHandle(ringbuf_handle_t handle)
{
    sound_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    rb_reset(handle);
    cmd.ptr = (void *)handle;
    cmd.mode = SOUNDPLAYER_MODE_MP3TTS;
    if(xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

int SoundPlayer::stopPlay()
{
    audio_pipeline_stop((audio_pipeline_handle_t)m_audioPipeline);
    audio_pipeline_wait_for_stop((audio_pipeline_handle_t)m_audioPipeline);
    return 0;
}

void SoundPlayer::play(const char* str, bool bSyncFlag)
{
    if(m_cmdQueue==NULL) return;

    ESP_LOGI(TAG, "Play sound %s.", str);
    sound_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    strcpy(cmd.str, str);
    cmd.mode = SOUNDPLAYER_MODE_MP3FILE;
    if(bSyncFlag) cmd.task = xTaskGetCurrentTaskHandle();
    if(xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))==pdTRUE) {
        if(bSyncFlag)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void SoundPlayer::speak(const char* str, bool bSyncFlag)
{
    char name[32];
    sprintf(name, "asr/%s", str);
    play(name, bSyncFlag);
}

void SoundPlayer::speakDigit(int num)
{
    char buff[8];
    sprintf(buff, "%d", num);
    char *p = buff;
    char name[32];
    while(*p!='\0') {
        if(*p=='-'){
            speak("EB-NUMNG");
        }else{
            sprintf(name, "asr/EB-NUM%c", *p);
            play(name);
        }
        p++;
    }
}

int SoundPlayer::buildNum(char *buff, int num)
{
    uint8_t len = 0;
    if(num==100) {
        sprintf(buff, ":asr/EB-NUMMX");
        len += 13;
        return len;
    }

    if(num/10 > 1) {
        sprintf(buff, ":asr/EB-NUM%d", num/10);
        len += 12;
    }

    if(num/10 >= 1) {
        sprintf(buff+len, ":asr/EB-NUM10");
        len += 13;
    }
    
    if(num%10 != 0 || num==0) {
        sprintf(buff+len, ":asr/EB-NUM%d", num%10);
        len += 12;
    }
    
    return len;
}

int SoundPlayer::speakWithNum(const char *name1, int num, const char *name2)
{
    sound_cmd_t cmd;
    uint8_t len = 0;
    BaseType_t rc = ESP_OK;
    memset(&cmd, 0, sizeof(cmd));
    cmd.mode = SOUNDPLAYER_MODE_MP3FILE;
    //cmd.voice_type = VOICE_TYPE_SPEACH;
    cmd.str[0] = '\0';

    sprintf(cmd.str, "asr/%s", name1);
    len = strlen(cmd.str);

    rc = buildNum(cmd.str+len, num);
    if(rc == ESP_FAIL) return rc;
    len += rc;

    if(name2!=NULL) {
        sprintf(cmd.str+len, ":asr/%s", name2);        
    }

    ESP_LOGI(TAG, "speakWithNum: name = %s", cmd.str);

    if(m_cmdQueue==NULL) return ESP_FAIL;
    rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(5000));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send sound play command: %s.", cmd.str);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void SoundPlayer::speakRspCode(int num)
{
    speak(VOICE_RSCODE);
    speakDigit(num);
}

void SoundPlayer::playEffect(int num)
{
    if(num==-1) {
        num = esp_random()%12;
    }
    char name[32];
    sprintf(name, "effects/%d", num);
    play(name);
}

void SoundPlayer::playEffect(const char *name)
{
    if(strcmp(name, ":SE:")==0) return playEffect(-1);

}