#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ringbuf.h"

class BD_STTService
{
private:
    ringbuf_handle_t m_ringBufferHandle;
    static void proc(void *ctx);
    int processAudioData();
public:
    int start();
};

