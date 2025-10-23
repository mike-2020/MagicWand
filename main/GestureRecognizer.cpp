#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "GestureClassifier.h"
#include "SoundPlayer.h"
#include "GestureRecognizer.h"
#include "GestureSensor.h"
#include "HMIMgr.h"
#include "MagicWand.h"

static const char *TAG = "GestureRecognizer";
static GestureRecognizer g_gestureRecognizer;
GestureRecognizer* GestureRecognizer::getInstance()
{
    return &g_gestureRecognizer;
}

void GestureRecognizer::recognizeCB(const char *label, void *ctx)
{
    if(label==NULL) {
        SoundPlayer::getInstance()->speak(VOICE_UNKNOWN_GESTURE1);
        return;
    }
    MagicWand::getInstance()->updateGesture(label);
}

void GestureRecognizer::proc(void *ctx)
{
    uint32_t cmd = 0;
    GestureRecognizer *handle = (GestureRecognizer*)ctx;
    while(1) {
        if (xQueueReceive(handle->m_cmdQueue, &cmd, pdMS_TO_TICKS(1000))!=pdTRUE) continue;
        ESP_LOGI(TAG, "Received inference command.");
        handle->inference();
    }
}

int GestureRecognizer::init()
{
    //create a queue to handle gpio event from isr
    m_cmdQueue = xQueueCreate(1, sizeof(uint32_t));

    xTaskCreate(GestureRecognizer::proc, "GestureRecognizer", 1024*6, (void*)this, 10, NULL);

    m_classiferHandle = new GestureClassifier(0.65, GestureRecognizer::recognizeCB, this);
    ((GestureClassifier*)m_classiferHandle)->setSampleCount(200);

    m_bIdleState = true;

    ESP_LOGI(TAG, "GestureRecognizer has been initialized.");
    return 0;
}

void GestureRecognizer::inference()
{
    float ax, ay, az;
    uint8_t n = 0;
    GestureClassifier *handle = (GestureClassifier *)m_classiferHandle;
    m_bIdleState = false;

    while(n++<200 && HMIMgr::getInstance()->getBtnState()==0) {
        GestureSensor::getInstance()->getAcceleration(&ax, &ay, &az);
        handle->appendData(ax, ay, az);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    handle->run();
    handle->reset();

    m_bIdleState = true;
}

void GestureRecognizer::sendCmd(uint8_t cmd)
{
    BaseType_t  rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(100));
    if(rc == pdTRUE ) {
        m_bIdleState = false;
    }else{
        m_bIdleState = true;
    }
}
