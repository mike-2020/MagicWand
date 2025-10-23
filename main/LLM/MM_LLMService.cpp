
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include <ArduinoJson.h>
#include "cJSON.h"
#include "MM_TTSService.h"
#include "MM_LLMService.h"

static const char *TAG = "MM_LLMService";
static const char *MM_URL_TPL = "https://api.minimax.chat/v1/text/chatcompletion_pro";
static const char *MM_PAYLOAD_TPL = "{ \
    \"bot_setting\":[             \
        {                       \
            \"bot_name\":\"神奇魔杖\",  \
            \"content\":\"你是一个神奇的魔杖，你可以理解小法师说的汉语拼音，并以正常语言做出回应。你的语言总是充满了神秘色彩。\"     \
        }                        \
    ],                          \
    \"reply_constraints\":{\"sender_type\":\"BOT\", \"sender_name\":\"神奇魔杖\"},  \
    \"model\":\"abab6.5s-chat\",    \
    \"stream\":false,              \
    \"tokens_to_generate\":2048,  \
    \"temperature\":0.01,         \
    \"top_p\":0.95,               \
    \"messages\":[]  \
    }";
static const char *MM_LLM_RSP_PREFIX = "\"reply\":\"";

char* MM_LLMService::buildRequest(const char *msg, int len)
{
    cJSON *pNodeRecvRoot= cJSON_Parse(MM_PAYLOAD_TPL);
    if(pNodeRecvRoot==NULL) ESP_LOGW(TAG, "Failed to parse json string: %s", MM_PAYLOAD_TPL);
    cJSON *pNodeMsgArray    = cJSON_GetObjectItem(pNodeRecvRoot, "messages");
    cJSON *pNodeMsgItem     = cJSON_CreateObject();
    cJSON_AddStringToObject(pNodeMsgItem, "sender_type", "USER");
    cJSON_AddStringToObject(pNodeMsgItem, "sender_name", "小法师");
    cJSON_AddStringToObject(pNodeMsgItem, "text", msg);
    cJSON_AddItemToArray(pNodeMsgArray, pNodeMsgItem);
    char *p = cJSON_Print(pNodeRecvRoot);
    cJSON_Delete(pNodeRecvRoot);
    return p;
}

void MM_LLMService::freeRequest(char *ptr)
{
    if(ptr!=NULL) cJSON_free(ptr);
}


int MM_LLMService::processResponse(int rspLen)
{
    int len = 0;
    int bufferLen = 1024;
    char buffer[1024];
    bool doneFlag = false;
    char *p = NULL;
    int outLen = 0;
    char *outText = (char*)malloc(OUTPUT_TEXT_MAX_LEN);

    len = esp_http_client_read_response(m_httpClient, buffer, bufferLen);
    p = strstr(buffer, MM_LLM_RSP_PREFIX);
    if(p==NULL) {
        ESP_LOGE(TAG, "Not found json prefix in the 1st block of the data.");
        free(outText);
        return ESP_FAIL;
    }
    p += strlen(MM_LLM_RSP_PREFIX);
    //printf("p-buffer=%d\n", p-buffer);
    while(1) {
        //printf("len = %d, p =%s\n", len, p);
        while(len - (p-buffer) >= 0 && outLen < OUTPUT_TEXT_MAX_LEN-1) {
            if(*p=='"') {
                doneFlag = true;
                break;
            }
            if(*p=='\\') {
                switch(*(p+1)) {
                    case 'n':
                        outText[outLen] = '\n';
                        break;
                    case 'r':
                        outText[outLen] = '\r';
                        break;
                    case 't':
                        outText[outLen] = '\t';
                        break;
                    case '"':
                        outText[outLen] = '"';
                        break;
                    case '\\':
                        outText[outLen] = '\\';
                        break;
                    case 'f':
                        outText[outLen] = '\f';
                        break;
                    case 'b':
                        outText[outLen] = '\b';
                        break;
                }
                p += 2;
            }else{
                outText[outLen] = *p;
                p++;
            }
            outLen++;
        }
        if(doneFlag==true)break;
        len = esp_http_client_read_response(m_httpClient, buffer, bufferLen);
        p = buffer;
    }
    outText[outLen] = '\0';

    ESP_LOGI(TAG, "Sending text to TTS: %s.", outText);
    MM_BaseService::getTTSInstance()->send(outText, true);
    return 0;
}

void MM_LLMService::proc(void *ctx)
{
    int rc = 0;
    MM_LLMService *handle = (MM_LLMService*)ctx;
    text_item_t txtItem;
    handle->m_bIsRunning = true;
    while(handle->m_bStopFlag!=true) {
        if (xQueueReceive(handle->m_cmdQueue, &txtItem, pdMS_TO_TICKS(1000))!=pdTRUE){
            continue;
        }
        ESP_LOGI(TAG, "Got text for LLM: %s", txtItem.ptr);
        rc = handle->submitHttpRequest(txtItem.ptr, txtItem.len);
        if(txtItem.freemem==true) {
            free(txtItem.ptr);
        }
        if(rc!=ESP_OK) {
            ESP_LOGE(TAG, "Failed to send text to LLM service(rc=%d).", rc);
        }
    }
    handle->m_bIsRunning = false;
    vTaskDelete(NULL);
}

int MM_LLMService::start()
{
    connect(MM_URL_TPL);
    m_cmdQueue = xQueueCreate(4, sizeof(text_item_t));

    m_bStopFlag = false;
    xTaskCreate(MM_LLMService::proc, "llm_svc", 1024*4, (void*)this, 5, NULL);
    return 0;
}

int MM_LLMService::stop()
{
    m_bStopFlag = true;

    if(m_cmdQueue!=NULL) vQueueDelete(m_cmdQueue);
    m_cmdQueue = NULL;
    return ESP_OK;
}