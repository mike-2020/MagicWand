#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
//#include "audio_common.h"
#include "audio_pipeline.h"
#include "audio_recorder.h"
#include "audio_mem.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "esp_vad.h"
#include "recorder_encoder.h"
#include "recorder_sr.h"
#include "model_path.h"
#include "esp_afe_sr_models.h"
#include "audio_idf_version.h"
#include "ASRTaskMgr.h"
#include "AudioMgr.h"
#include "ASRService.h"
#include "esp_heap_caps.h"
#include "ADFNotSupportCPP.h"
#include "STTService.h"



static ASRService g_asrService;
//static volatile int task_flag = 1;
srmodel_list_t *models = NULL;
static const char *TAG = "ASRService";
static audio_element_handle_t raw_read      = NULL;
static esp_afe_sr_iface_t *afe_handle = NULL;
static esp_mn_iface_t *multinet = NULL;
static model_iface_data_t *model_data = NULL;

#if 0
static esp_err_t rec_engine_cb(audio_rec_evt_t *event, void *user_data)
{
    if (audio_rec_evt_t::AUDIO_REC_WAKEUP_START == event->type) {
        recorder_sr_wakeup_result_t *wakeup_result = (recorder_sr_wakeup_result_t*)event->event_data;

        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_START");
        ESP_LOGI(TAG, "wakeup: vol %f, mod idx %d, word idx %d", wakeup_result->data_volume, wakeup_result->wakenet_model_index, wakeup_result->wake_word_index);

    } else if (audio_rec_evt_t::AUDIO_REC_VAD_START == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_START");

    } else if (audio_rec_evt_t::AUDIO_REC_VAD_END == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_STOP");

    } else if (audio_rec_evt_t::AUDIO_REC_WAKEUP_END == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_END");
        AUDIO_MEM_SHOW(TAG);
    } else if (audio_rec_evt_t::AUDIO_REC_COMMAND_DECT <= event->type) {
        recorder_sr_mn_result_t *mn_result = (recorder_sr_mn_result_t*)event->event_data;

        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_COMMAND_DECT");
        ESP_LOGW(TAG, "command %d, phrase_id %d, prob %f, str: %s"
            , event->type, mn_result->phrase_id, mn_result->prob, mn_result->str);

    } else {
        ESP_LOGE(TAG, "Unkown event");
    }
    return ESP_OK;
}

static int input_cb_for_afe(int16_t *buffer, int buf_sz, void *user_ctx, TickType_t ticks)
{
    //ESP_LOGI(TAG, "buf_sz=%d", buf_sz);
    return raw_stream_read(raw_read, (char *)buffer, buf_sz);
}
#endif

 ASRService* ASRService::getInstance()
 {
    return &g_asrService;
 }

int ASRService::setupRecordPipeline()
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_reader, filter;

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    //rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    //rsp_cfg.src_rate = 16000;
    //rsp_cfg.dest_rate = 16000;
    //filter = rsp_filter_init(&rsp_cfg);
    //audio_pipeline_register(pipeline, filter, "filter");

    ESP_LOGI(TAG, "[2.1] Create i2s stream to read audio data from codec chip");
    i2s_stream_reader = build_i2s_stream(AUDIO_STREAM_READER);

    ESP_LOGI(TAG, "[2.3] Create raw to receive data");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[ 3 ] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, raw_read, "raw");

    ESP_LOGI(TAG, "[ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[VAD]");
    const char *link_tag[2] = {"i2s", "raw"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    return audio_pipeline_run(pipeline);
}

int ASRService::initModel()
{

    models = esp_srmodel_init("model");
    char *wn_name = NULL;
    char *wn_name_2 = NULL;

    if (models!=NULL) {
        for (int i=0; i<models->num; i++) {
            if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
                if (wn_name == NULL) {
                    wn_name = models->model_name[i];
                    printf("The first wakenet model: %s\n", wn_name);
                } else if (wn_name_2 == NULL) {
                    wn_name_2 = models->model_name[i];
                    printf("The second wakenet model: %s\n", wn_name_2);
                }
            }
        }
    } else {
        printf("Please enable wakenet model and select wake word by menuconfig!\n");
        return -1;
            }

    ESP_LOGI(TAG, "Creating ASR models from config...");

    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = build_afe_config();
    afe_config.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    afe_config.wakenet_init = true;
    afe_config.wakenet_model_name = wn_name;
    afe_config.wakenet_model_name_2 = wn_name_2;
    afe_config.voice_communication_init = false;
    afe_config.se_init = true;

    afe_config.vad_mode = VAD_MODE_2;
    afe_config.aec_init = false;
    afe_config.agc_mode = AFE_MN_PEAK_NO_AGC;
    afe_config.pcm_config.total_ch_num = (AUDIO_MIC_CHAN_NUM+0);
    afe_config.pcm_config.mic_num = AUDIO_MIC_CHAN_NUM;
    afe_config.pcm_config.ref_num = 0;

    esp_afe_sr_data_t *afeData = NULL;
    afeData = afe_handle->create_from_config(&afe_config);
    if(afeData==NULL) return -1;
    m_afeData = (void *)afeData;


    //int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    ESP_LOGI(TAG, "multinet: %s", mn_name);
    multinet = esp_mn_handle_from_name(mn_name);
    model_data = multinet->create(mn_name, m_nWakeTimeOut);  //1min for timeout

    ASRTaskMgr::getInstance()->updateCmdList();

    ESP_LOGI(TAG, "Successfully initialized ASR models.");
    return 0;
}

#define ADC_I2S_CHANNEL 2
void ASRService::feedTask(void *ctx)
{
    ASRService *handle = (ASRService*)ctx;
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t*)handle->m_afeData;

    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_channel_num(afe_data);
    int total_ch_num = afe_handle->get_total_channel_num(afe_data);
    //int nFeedChannel = esp_get_feed_channel();

    ESP_LOGI(TAG, "*****nch=%d, total_ch_num=%d, audio_chunksize=%d*****", nch,  total_ch_num, audio_chunksize);
    int nBuffLen = audio_chunksize * sizeof(int16_t) * total_ch_num;
    int16_t *pI2sBuff = (int16_t *)malloc(nBuffLen);
    assert(pI2sBuff);
    int rc = 0;

    while (1) {
        //for ASR command detect
        rc = raw_stream_read(raw_read, (char *)pI2sBuff, nBuffLen);
        if(rc!=nBuffLen) {
            ESP_LOGW(TAG, "Failed to read raw PCM data(nBuffLen=%d, rc=%d)", nBuffLen, rc);
            continue;
        }
        afe_handle->feed(afe_data, pI2sBuff);
    }
    if (pI2sBuff) {
        free(pI2sBuff);
        pI2sBuff = NULL;
    }
    vTaskDelete(NULL);
}

void ASRService::detectTask(void *ctx)
{
    ASRService *handle = (ASRService*)ctx;
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t*)handle->m_afeData;

    //asr_voiceprint_buffer_init();
    ESP_LOGD(TAG, "------------detect start------------\n");

    while (1) {
        afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "fetch error!");
            break;
        }

        //for LLM speech
        if(handle->m_bLLMSpeechStopFlag==false) {
            if(handle->m_bLLMSpeechRunning==false) {
                ESP_LOGI(TAG, "LLM speech is started...");
                handle->m_bLLMSpeechRunning = true;
                handle->m_nLLMSpeechStopDelayCount = 30;  //delay stop for 30ms * 30 = 0.9s. This is to ensure the audio in buffer are captured
            }
            STTService::getInstance()->captureSpeech((char*)res->data, res->data_size);
            continue; //if LLM speech is enabled, then the ASR should not process the same audio data again.
        }else{
            if(handle->m_bLLMSpeechRunning==true && handle->m_nLLMSpeechStopDelayCount--==0) {
                ESP_LOGI(TAG, "LLM speech is running, but stop flag is set. Sending speech end signal...");
                STTService::getInstance()->captureSpeech(NULL, 0); //send end flag
                handle->m_bLLMSpeechRunning = false;
            }
        }

        //check if ASR should detect command in voice
        if(handle->m_bASRListenFlag==false) continue;


        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "wakeword detected\n");
	        ESP_LOGI(TAG, "model index:%d, word index:%d, data len:%d\n", res->wakenet_model_index, res->wake_word_index, res->data_size);
            ESP_LOGD(TAG, "-----------LISTENING-----------\n");
            multinet->clean(model_data);  // clean all status of multinet

            
            
        }else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {

            handle->m_bWakeCmdDetected = true;
            ASRTaskMgr::getInstance()->onWakeup();
            //printf("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d, frame_count=%d\n", res->trigger_channel_id, frame_count);
            //printf("asr_vp_input_data_len = %d\n", asr_vp_input_data_len);
            //if(asr_vp_input_buffer != NULL && asr_vp_input_data_len>0) {
                //int speaker_index = asr_get_voiceprint_registration_mode();
                //size_t mem = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
                //ESP_LOGI(TAG, "Free memory left2: %d.", mem);
                //if(speaker_index == ASR_VOICEPRINT_NONE) {
                    //char speaker[16];
                    //int rc = speaker_query(asr_vp_input_buffer, asr_vp_input_data_len, speaker);
                    //if(rc!=ESP_OK){
                    //    strcpy(speaker, "主人");
                    //}
                    //tts_speak(speaker);
                    //tts_speak("你好。");
                //}else{
                    //int rc = speaker_enroll(asr_vp_input_buffer, asr_vp_input_data_len, speaker_index);
                    //if(rc==ESP_FAIL) {
                    //    asr_set_voiceprint_registration_mode(ASR_VOICEPRINT_NONE);
                        //tts_speak("语音登记失败。");
                    //}else if(rc==ESP_OK){
                    //    asr_set_voiceprint_registration_mode(ASR_VOICEPRINT_NONE);
                        //tts_speak("语音登记成功完成。");
                    //}else{
                        //tts_speak("非常好，请再说一遍“你好，小志”");
                    //}
                //}
                //asr_voiceprint_buffer_reset();
            //}
        }

        if (handle->m_bWakeCmdDetected==true || handle->m_bForceCmdDetect==true) {

            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING) {
                //esp_mn_results_t *mn_result = multinet->get_results(model_data);
                //printf("mn_result->string=%s\n", mn_result->string);
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                ESP_LOGD(TAG, "mn_result->string=%s\n", mn_result->string);
                for (int i = 0; i < mn_result->num; i++) {
                    ESP_LOGD(TAG, "TOP %d, command_id: %d, phrase_id: %d, string:%s prob: %f\n", 
                    i+1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
                }
                if(mn_result->num>0){ 
                    int rc = ASRTaskMgr::getInstance()->onCmdReceived(mn_result->command_id[0]);
                    if(rc==ASR_CMD_EXIT_WAKEUP) {
                        afe_handle->enable_wakenet(afe_data);
                        handle->m_bWakeCmdDetected = false;
                    }
                    //asr_voiceprint_buffer_reset();
                }
                ESP_LOGD(TAG, "\n-----------listening-----------\n");
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                ESP_LOGD(TAG, "timeout, string:%s\n", mn_result->string);
                afe_handle->enable_wakenet(afe_data);
                handle->m_bWakeCmdDetected = false;
                handle->m_bForceCmdDetect = false;

                ASR_TASK_EXIT_WAKEUP();
                //asr_set_voiceprint_registration_mode(ASR_VOICEPRINT_NONE);
                //asr_voiceprint_buffer_reset();
                ESP_LOGD(TAG, "\n-----------awaits to be waken up-----------\n");
                continue;
            }
        }
    }
    vTaskDelete(NULL);
}

 
int ASRService::init()
{
    int rc = 0;

    m_nWakeTimeOut = 1*60*1000;
    m_bWakeCmdDetected = false;
    m_bVADDetected = false;
    m_bASRListenFlag = true;
    m_bForceCmdDetect = false;
    //make LLM speech in stop state
    m_bLLMSpeechStopFlag = true;
    m_bLLMSpeechRunning = false;

    ESP_LOGI(TAG, "asr_service_init started.");
    rc = setupRecordPipeline();
    if(rc<0) return rc;

    rc = initModel();
    if(rc<0) return rc;

    m_bInitialied = true;

    xTaskCreatePinnedToCore(ASRService::feedTask, "feed_task", 8 * 1024, (void*)this, 5, NULL, 0);
    xTaskCreatePinnedToCore(ASRService::detectTask, "detect_task", 5 * 1024, (void*)this, 5, NULL, 1);  
    
    return 0;
}