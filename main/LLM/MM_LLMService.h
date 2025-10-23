#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "MM_BaseService.h"


#define OUTPUT_TEXT_MAX_LEN     2048


class MM_LLMService : public MM_BaseService
{
private:
    virtual int processResponse(int rspLen);
    virtual char* buildRequest(const char *msg, int len);
    virtual void freeRequest(char *ptr);
    static void proc(void *ctx);
public:
    virtual int start();
    virtual int stop();
};