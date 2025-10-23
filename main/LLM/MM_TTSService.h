#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ringbuf.h"
#include "MM_BaseService.h"

typedef struct{
    char *txt;
    int len;
    bool need_freemem;
}tts_text_t;

class MM_TTSService : public MM_BaseService
{
private:
    ringbuf_handle_t m_ringBufferHandle;
    const char *m_curText2TTS;
    static void proc(void *ctx);
    virtual int processResponse(int rspLen);
    virtual char* buildRequest(const char *txt, int len);
    virtual void freeRequest(char *ptr);
    virtual int submitHttpRequest(char *reqMsg, int len);
    int processTextInput(char *txt, size_t len);
public:
    virtual int start();
    virtual int stop();
};

