#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "GestureSensor.h"
#include "SampleRecorder.h"
#include "SoundPlayer.h"
#include "GestureClassifier.h"
#include "HMIMgr.h"
#include "IngestionEI.h"
#include "ASRTaskMgr.h"


SampleRecorder g_sampleRecorder;
static const char *TAG = "SampleRecorder";


SampleRecorder* SampleRecorder::getInstance()
{
    return &g_sampleRecorder;
}

SampleRecorder::SampleRecorder()
{
    //create a queue to handle cmd
    m_cmdQueue = xQueueCreate(1, sizeof(training_cmd_t));
    m_stopRecordFlag = true;
    m_bIsRunning = false;
    m_bIdleState = true;
}

int SampleRecorder::start()
{
    SoundPlayer::getInstance()->speak(VOICE_SAMPLE_HELP);
    if(m_stopRecordFlag==false) return -1;
    ESP_LOGI(TAG, "Starting Sample Recorder...");
    //start gpio task
    m_stopRecordFlag = false;
    xTaskCreate(SampleRecorder::proc, "SampleRecorder", 4096, (void*)this, 10, NULL);

    ASRTaskMgr::getInstance()->updateCmdList(WORKING_MODE_TRAINING);
    return 0;
}

int SampleRecorder::sendCmd(training_cmd_t *cmd)
{
    if(m_bIsRunning == false) return ESP_FAIL;

    return xQueueSend(m_cmdQueue, cmd, pdMS_TO_TICKS(100));
}

int SampleRecorder::setLabel(const char *label)
{
    ESP_LOGI(TAG, "label=%s", label);
    training_cmd_t cmd;
    cmd.cmd = SR_CMD_SETLABEL;
    strcpy(cmd.data, label);
    return sendCmd(&cmd);
}



void SampleRecorder::proc(void *ctx)
{
    SampleRecorder *handle = (SampleRecorder*)ctx;
    IngestionEI ingestor;
    training_cmd_t cmd;
    handle->m_bIsRunning = true;
    strcpy(handle->m_curLabel, "idle");
    reportGestureName(handle->m_curLabel);
    handle->m_pDataBuffer = (float(*)[3])malloc((sizeof(float)*3)*200*3);
    ingestor.init();
    while(handle->m_stopRecordFlag==false) {
        if (xQueueReceive(handle->m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE) continue;
        ESP_LOGW(TAG, "Received cmd %d.", cmd.cmd);
        switch(cmd.cmd) {
            case SR_CMD_TRIGGER:
                handle->record(&ingestor);
                break;
            case SR_CMD_SETLABEL:
                strcpy(handle->m_curLabel, cmd.data);
                reportGestureName(handle->m_curLabel);
                break;
        }
        
    }
    ingestor.deinit();
    free(handle->m_pDataBuffer);
    ESP_LOGI(TAG, "SampleRecorder exiting...");
    handle->m_bIsRunning = false;
    vTaskDelete(NULL);
}




void SampleRecorder::record(void *ingestor)
{
    float ax, ay, az;
    uint8_t n = 0;
    int rc = 0;
    m_bIdleState = false;

    //FILE *fp = fopen("/data/r1-s1.dat", "w+");
    //if(fp==NULL) {
    //    ESP_LOGE(TAG, "Failed to open data file.");
    //    return;
    //}
    HMIMgr::getInstance()->turnOnLED();
    //SoundPlayer::getInstance()->speak(VOICE_SAMPLE_START);

    while(m_stopRecordFlag==false && n<200 && HMIMgr::getInstance()->getBtnState()==0) {
        GestureSensor::getInstance()->getAcceleration(&m_pDataBuffer[n][0], &m_pDataBuffer[n][1], &m_pDataBuffer[n][2]);
        n++;
        //ESP_LOGI(TAG, "AX=%d, AY=%d, AZ=%d.", ax, ay, az);
        //fprintf(fp, "%d %d %d\n", ax, ay, az);

        //GestureSensor::getInstance()->getRotation(&ax, &ay, &az);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    //fclose(fp);

    if(n>=200) {
        SoundPlayer::getInstance()->speak(VOICE_SAMPLE_TMOUT);   //discard sample data if it was timeout.
        goto out;
    }else{
        SoundPlayer::getInstance()->speak(VOICE_SAMPLE_END);
        reportGestureName(m_curLabel);
    }

    if(n<50) {
        ESP_LOGW(TAG, "data too short.");
        SoundPlayer::getInstance()->speak(VOICE_TRN_DATA_LESS);
        goto out;
    }
    HMIMgr::getInstance()->turnOffLED();
    HMIMgr::getInstance()->startLEDBlink();
    rc = ((IngestionEI*)ingestor)->ingest(m_curLabel, m_pDataBuffer, n);
    HMIMgr::getInstance()->stopLEDBlink();
    if(rc==ESP_OK) {
        SoundPlayer::getInstance()->speak(VOICE_TRN_SUBMIT_SUCCESS);
    }else{
        SoundPlayer::getInstance()->speak(VOICE_TRN_SUBMIT_FAIL);
    }
out:
    m_bIdleState = true;
}