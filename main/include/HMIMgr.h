#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "iot_button.h"
#include "CommonBase.h"

#define WORKING_MODE_TRAINING   1
#define WORKING_MODE_WAND       2
#define WORKING_MODE_CONTROLLER   3

#define HMI_CMD_BTN_PRESS       1
#define HMI_CMD_MODE_NORMAL     2
#define HMI_CMD_MODE_TRAINING   3
#define HMI_CMD_MODE_CONTROLLER   4


#define BTN_NUM_CENTER      0
#define BTN_NUM_LEFT        1
#define BTN_NUM_RIGHT       2
#define BTN_NUM_UP          3
#define BTN_NUM_DOWN        4

typedef void(*hmi_btn_cb_t)(void*);

typedef struct {
    void *ctx;
    hmi_btn_cb_t cb_pressdown;
    hmi_btn_cb_t cb_pressup;
    hmi_btn_cb_t cb_click;
    hmi_btn_cb_t cb_dbclick;
}hmi_btn_t;



class HMIMgr {
private:
    WMBase *m_curWMInstance;

    static void onBtnCenterPressDown(void *btnHandle, void *ctx);
    static void onBtnCenterPressUp(void *btnHandle, void *ctx);
    static void onBtnLeftClick(void *btnHandle, void *ctx);
    static void onBtnRightClick(void *btnHandle, void *ctx);
    
public:
    QueueHandle_t m_cmdQueue;
    int m_nWorkingMode;
    bool m_bLEDBlink;
    static void proc(void *ctx);
    void onBtnPressed();
    void setWorkingMode(int wMode);
    void blinkLED();
public:
    HMIMgr();
    int init();
    int getBtnState();
    int sendCmd(uint32_t cmd);
    void startLEDBlink(){m_bLEDBlink=true;}
    void stopLEDBlink(){m_bLEDBlink=false; turnOffLED();}
    void turnOnLED();
    void turnOffLED();
    void registerBtnEvent(int btnNum, button_event_t evtType, hmi_btn_cb_t cb, void *ctx);
    static HMIMgr* getInstance();
};


extern "C" {
void board_led_turn_off();
void board_led_turn_on();
}


