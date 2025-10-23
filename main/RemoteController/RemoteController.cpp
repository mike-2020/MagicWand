#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "GestureSensor.h"
#include "RemoteController.h"

int ble_main(void);
void send_mouse(uint8_t buttons, char dx, char dy, char wheel);
void send_battery(uint8_t level);

static const char *TAG = "RemoteController";
static RemoteController g_remoteController;

RemoteController* RemoteController::getInstance()
{
    return &g_remoteController;
}

int RemoteController::start()
{
    
    m_stopRecordFlag = false;
    xTaskCreatePinnedToCore(RemoteController::proc, "RemoteController", 1024*8, (void*)this, 10, NULL, 0);
    return 0;
}

void RemoteController::proc(void *ctx)
{
    RemoteController *handle = (RemoteController*)ctx;

    ble_main();

    while(handle->m_stopRecordFlag==false) {
        if(handle->m_bReportData==true) {
            handle->mouse();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "RemoteController exiting...");
    //handle->m_bIsRunning = false;
    vTaskDelete(NULL);
}

int RemoteController::mouse()
{
    float ax, ay, az;
    float v, h, s;
    //GestureSensor::getInstance()->getTransformedMotionData(&ax, &ay, &az);
    //ESP_LOGI(TAG, "ax = %.3f, ay = %.3f, az = %.3f", ax, ay, az);
    //GestureSensor::getInstance()->getDataForMouse(&v, &h, &s);
    //int8_t dx = (ax * 5);
    //int8_t dy = (az * 5);
    //printf("dx=%d dy=%d\n", dx, dy);
    int16_t dx = (h * 20);
    int16_t dy = (v * 20);
    
    GestureSensor::getInstance()->getDataForMouse(&dy, &dx, NULL);
    if(dx!=0 || dy!=0) {
        printf("dx=%d h=%.3f dy=%d v=%.3f\n", dx, h, dy, v);
        send_mouse(0, dx, dy, 0);
    }
    return 0;
}

void RemoteController::onHostConnected()
{
    send_battery(50);
    this->enableDataReport();
}

void RemoteController::onHostDisconnected()
{
    this->disableDataReport();
}
