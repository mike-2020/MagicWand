#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"

typedef struct{
    char *ptr;
    int len;
    bool freemem;
}text_item_t;


class MM_BaseService{
protected:
    QueueHandle_t m_cmdQueue;
    esp_http_client_handle_t m_httpClient;
    bool m_bStopFlag;
    bool m_bIsRunning;
protected:
    int connect(const char *url);
    virtual int submitHttpRequest(char *reqMsg, int len);
    int doHttpRequest(char *reqMsg, int len);

    virtual int processResponse(int rspLen) = 0;
    virtual char* buildRequest(const char *txt, int len)=0;
    virtual void freeRequest(char *ptr)=0;

public:
    MM_BaseService();
    int send(char *txt, bool bZeroCopy=false);
    virtual int start() = 0;
    virtual int stop() = 0;
    static MM_BaseService* getLLMInstance();
    static MM_BaseService* getTTSInstance();
};
