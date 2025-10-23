
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "MagicWand.h"
#include "GestureRecognizer.h"
#include "GestureCharmCommon.h"
#include "SoundPlayer.h"
#include "ASRTaskMgr.h"
#include "IRMTransceiver.h"
#include "ASRService.h"
#include "HMIMgr.h"

static MagicWand g_magicWand;
static IRMTransceiver g_irmTrans;
static const char *TAG = "MagicWand";

MagicWand* MagicWand::getInstance()
{
    return &g_magicWand;
}

MagicWand::MagicWand()
{
    //create a queue to handle cmd
    m_cmdQueue = xQueueCreate(1, sizeof(gesture_cmd_t));
    m_stopRecordFlag = true;
    m_bIsRunning = false;
}

int MagicWand::start()
{
    //SoundPlayer::getInstance()->speak(VOICE_SAMPLE_HELP);
    if(m_stopRecordFlag==false) return -1;
    ESP_LOGI(TAG, "Starting Wand mode...");
    //start gpio task
    m_stopRecordFlag = false;
    xTaskCreate(MagicWand::proc, "MagicWand", 4096, (void*)this, 10, NULL);

    ASRTaskMgr::getInstance()->updateCmdList(WORKING_MODE_WAND);


    HMIMgr::getInstance()->registerBtnEvent(BTN_NUM_CENTER, BUTTON_PRESS_DOWN, MagicWand::onBtnCenterPressDown, this);
    HMIMgr::getInstance()->registerBtnEvent(BTN_NUM_CENTER, BUTTON_PRESS_UP, MagicWand::onBtnCenterPressUp, this);
    return 0;
}

int MagicWand::stop()
{
    WMBase::stop();
    return 0;
}

void MagicWand::proc(void *ctx)
{
    MagicWand *handle = (MagicWand*)ctx;
    gesture_cmd_t cmd;
    handle->m_bIsRunning  = true;
    while(1) {

        if (xQueueReceive(handle->m_cmdQueue, &cmd, pdMS_TO_TICKS(200))!=pdTRUE) continue;
        ESP_LOGW(TAG, "Received command %d.", cmd.cmd);
        switch(cmd.cmd) {
            case 1:
                handle->onGestureReceived(cmd.data);
                break;
            case 2:
                handle->onCharmReceived(cmd.strPtr);
                break;
        }
        
    }

    handle->m_bIsRunning = false;
    ESP_LOGI(TAG, "MagicWand exiting...");
    vTaskDelete(NULL);
}


int MagicWand::updateGesture(const char *name)
{
    ESP_LOGI(TAG, "gesture name: %s.", name);
    if(m_bIsRunning==false) return -1;
    gesture_cmd_t cmd;
    cmd.cmd = 1;
    strcpy(cmd.data, name);
    BaseType_t rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(100));
    return rc;
}

int MagicWand::updateCharm(const char *msg)
{
    if(m_bIsRunning==false) return -1;
    gesture_cmd_t cmd;
    cmd.cmd = 2;
    cmd.strPtr = msg;
    BaseType_t rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(100));
    return rc;
}

void MagicWand::onCharmReceived(const char *charmMsg)
{
    int rc = 0;
    const char *musicLabel = NULL;
    uint16_t irmCode = 0, lightStyle = 0;
    rc = queryActionsByCharm(charmMsg, &musicLabel, &irmCode, &lightStyle);
    if(rc < 0) {
        SoundPlayer::getInstance()->speak(VOICE_CHARM_ERROR);
        return;
    }

    if(musicLabel!=NULL) {
        SoundPlayer::getInstance()->playEffect(musicLabel);
    }
    if(irmCode!=0) {
        g_irmTrans.send(irmCode>>8, irmCode&0xFF);
    }
}

void MagicWand::onGestureReceived(const char *label)
{
    int rc = 0;
    reportGestureName(label);
    rc = verifyAndUpdateGesture(label);
    if(rc<0) {
        SoundPlayer::getInstance()->speak(VOICE_NO_GESTURE);
        return;
    }
    ASRService::getInstance()->enableForceCmdDetect();
    
    const char *musicLabel = NULL;
    uint16_t irmCode = 0, lightStyle = 0;
    rc = queryActionsByGesture(label, &musicLabel, &irmCode, &lightStyle);
    if(rc < 0) {
        SoundPlayer::getInstance()->speak(VOICE_NEED_CHARM);
        return;
    }

    if(musicLabel!=NULL) {
        SoundPlayer::getInstance()->playEffect(musicLabel);
    }

    if(irmCode!=0) {
        g_irmTrans.send(irmCode>>8, irmCode&0xFF);
    }
}


void MagicWand::onBtnCenterPressDown(void *ctx)
{
    MagicWand *handle = (MagicWand*)ctx;
    ESP_LOGI(TAG, "Button center pressed down.");
    ASRService::getInstance()->llmSpeechStart();
}

void MagicWand::onBtnCenterPressUp(void *ctx)
{
    MagicWand *handle = (MagicWand*)ctx;
    ESP_LOGI(TAG, "Button center pressed up.");
    ASRService::getInstance()->llmSpeechEnd();
}