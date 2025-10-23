#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "GestureSensor.h"
#include "GestureCharmCommon.h"
#include "CommonBase.h"

#define SR_CMD_TRIGGER      1
#define SR_CMD_SETLABEL     2

typedef gesture_cmd_t  training_cmd_t;

class SampleRecorder : public WMBase
{
private:
    float (*m_pDataBuffer)[3];
    char m_curLabel[32];
private:
    static void proc(void *ctx);
    void record(void *ingestor);
public:
    QueueHandle_t m_cmdQueue;
    bool m_bIsRunning;
public:
    SampleRecorder();
    virtual int start();
    bool getIdleState() { return m_bIdleState; };
    int sendCmd(training_cmd_t *cmd);
    int setLabel(const char *label);
    static SampleRecorder* getInstance();
};

