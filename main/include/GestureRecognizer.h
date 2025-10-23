#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define GR_CMD_TRIGGER      1


class GestureRecognizer {
private:
    QueueHandle_t m_cmdQueue;
    void *m_classiferHandle;
    bool m_bIdleState;
    static void recognizeCB(const char *label, void *ctx);
    static void proc(void *ctx);
    void inference();
public:
    int init();
    void sendCmd(uint8_t cmd);
    bool getIdleState() { return m_bIdleState; };
    static GestureRecognizer* getInstance();
};

