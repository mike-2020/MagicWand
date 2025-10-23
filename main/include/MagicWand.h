#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "CommonBase.h"

class MagicWand : public WMBase{
private:
    QueueHandle_t m_cmdQueue;
    bool m_bIsRunning;
    static void proc(void *ctx);
public:
    MagicWand();
    virtual int start();
    virtual int stop();
    int sendCmd();
    int updateGesture(const char *name);
    int updateCharm(const char *msg);

    void onCharmReceived(const char *charmMsg);
    void onGestureReceived(const char *label);

    static void onBtnCenterPressDown(void *ctx);
    static void onBtnCenterPressUp(void *ctx);

    static MagicWand* getInstance();
};
