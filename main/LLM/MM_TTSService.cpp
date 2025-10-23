
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "SoundPlayer.h"
#include "MM_TTSService.h"
#include "MBaseUI.h"

static const char *TAG = "MM_TTSService";
static const char *MM_URL_TPL = "https://api.minimax.chat/v1/t2a_v2";
static const char *MM_TTS_REQ_TPL = "{ \
    \"model\": \"speech-01-turbo\",   \
    \"text\": \"\",           \
    \"voice_setting\":{         \
        \"voice_id\": \"male-qn-qingse\",     \
        \"speed\": 1,           \
        \"vol\": 1,             \
        \"pitch\": 0            \
    },                          \
    \"audio_setting\":{         \
        \"sample_rate\": 16000, \
        \"bitrate\": 32000,    \
        \"format\": \"mp3\"     \
    }}";
static const char *MM_TTS_RSP_PREFIX = "\"audio\":\"";


static char hexchr2bin(const char hex)
{
	if (hex >= '0' && hex <= '9') {
		return hex - '0';
	} else if (hex >= 'A' && hex <= 'F') {
		return hex - 'A' + 10;
	} else if (hex >= 'a' && hex <= 'f') {
		return hex - 'a' + 10;
	} else {
        ESP_LOGE(TAG, "HEX decoding error: 0x%X.", hex);
		return 0;
	}
}

int MM_TTSService::processResponse(int rspLen)
{
    int len = 0;
    int bufferLen = 1024;
    char buffer[1024];
    char *pStart = buffer;
    char *pBin = buffer;
    char *p = NULL;
    bool doneFlag = false;

    //show the text for current TTS result
    ((MProphetUI*)MUIMgr::getInstance()->getCurUIHandle())->appendText(m_curText2TTS);

    len = esp_http_client_read_response(m_httpClient, buffer, bufferLen);
    //buffer[len] = '\0';
    //printf("buffer=%s\n", buffer);
    p = strstr(buffer, MM_TTS_RSP_PREFIX);
    if(p==NULL) {
        buffer[len-1] = '\0';
        ESP_LOGE(TAG, "Not found json prefix in the 1st block of the data: %s.", buffer);
        return ESP_FAIL;
    }

    //skip response prefix (p will point the 1st byte of incoming audio data)
    p += strlen(MM_TTS_RSP_PREFIX);
    //printf("p-buffer=%d\n", p-buffer);
    while(1) {
        //printf("len = %d, p =%s\n", len, p);
        while(len - (p-buffer) >= 2) {
            if(*p=='"' || *(p+1)=='"') {
                if(*(p+1)=='"') ESP_LOGW(TAG, "Something wrong with data processing.");
                doneFlag = true;
                break;
            }
            *pBin = (hexchr2bin(*p) << 4) | hexchr2bin(*(p+1));
            p += 2;
            pBin++;
        }
        //printf("\np-buffer=%d\n", p-buffer);
        //ESP_LOGI(TAG, "Writing data to ring buffer: %d.", pBin - buffer);
        rb_write(m_ringBufferHandle, buffer, pBin - buffer, portMAX_DELAY);
        if(doneFlag==true) break;
        if(len - (p-buffer)==1) { //still there is one char left in the buffer not processed?
            buffer[0] = *p;
            pStart = buffer + 1;
            ESP_LOGW(TAG, "Something wrong with data processing.");
        }
        len = esp_http_client_read_response(m_httpClient, pStart, bufferLen-(pStart-buffer));
        len += (pStart-buffer);
        pBin = buffer;
        p = buffer;
    }

    return 0;
}

char* MM_TTSService::buildRequest(const char *txt, int len)
{
    cJSON *pNodeRecvRoot= cJSON_Parse(MM_TTS_REQ_TPL);
    if(pNodeRecvRoot==NULL) ESP_LOGW(TAG, "Failed to parse json string: %s", cJSON_GetErrorPtr());
    cJSON * pNodeTextVal = cJSON_CreateStringReference(txt);
    if(pNodeTextVal==NULL) ESP_LOGW(TAG, "Failed to create CJSON object.");
    bool rc = cJSON_ReplaceItemInObject(pNodeRecvRoot, "text", pNodeTextVal);
    if(rc==false) ESP_LOGW(TAG, "Failed to replace CJSON object.");
    char *p = cJSON_Print(pNodeRecvRoot);
    cJSON_Delete(pNodeRecvRoot);
    return p;
}

void MM_TTSService::freeRequest(char *ptr)
{
    if(ptr!=NULL) cJSON_free(ptr);
}

int MM_TTSService::submitHttpRequest(char *reqMsg, int len)
{
    m_curText2TTS = reqMsg;
    return MM_BaseService::submitHttpRequest(reqMsg, len);
}

int MM_TTSService::processTextInput(char *txt, size_t len)
{
    int n = 0;
    char *p = txt, *pStart = txt;
    char *PERIOD = "。";
    size_t PERIOD_LEN = strlen(PERIOD);
    char *SEMICOLON = "；";
    size_t SEMICOLON_LEN = strlen(SEMICOLON);
    char *COLON = "：";
    size_t COLON_LEN = strlen(COLON);
    char *COMMA = "，";
    size_t COMMA_LEN = strlen(COMMA);
    while(*p!='\0') {
        //try to avoid short sentens because frequncy threshold is 3 requests per min.
        if(n<350) {
            p++;
            n++;
            continue;
        }
        ESP_LOGI(TAG, " n = %d.", n);

        if(strncmp(p, PERIOD, PERIOD_LEN)==0) {
            //n += PERIOD_LEN;
            *p = '\0';
            submitHttpRequest(pStart, n);
            pStart += (n + PERIOD_LEN);
            p += PERIOD_LEN;
            n = 0;
        }else if(strncmp(p, SEMICOLON, SEMICOLON_LEN)==0) {
            //n += SEMICOLON_LEN;
            *p = '\0';
            submitHttpRequest(pStart, n);
            pStart += (n + SEMICOLON_LEN);
            p += SEMICOLON_LEN;
            n = 0;
        }else if(strncmp(p, COLON, COLON_LEN)==0) {
            //n += COLON_LEN;
            *p = '\0';
            submitHttpRequest(pStart, n);
            pStart += (n + COLON_LEN);
            p += COLON_LEN;
            n = 0;
        }else if(strncmp(p, COMMA, COMMA_LEN)==0) {  //sentens is too long, break at comma
            //n += COMMA_LEN;
            *p = '\0';
            submitHttpRequest(pStart, n);
            pStart += (n + COMMA_LEN);
            p += COMMA_LEN;
            n = 0;
        }else if(*p=='\n' || *p=='\r') {
            if(n>0) {               //only submit for TTS if pStart points some content
                *p = '\0';
                submitHttpRequest(pStart, n);
            }
            pStart += (n + 1);
            p += 1;
            n = 0;
        }else{
            p++;
            n++;
        }
        if(n==500) { //Reach to request uplimit of MinMax.
            submitHttpRequest(pStart, n);
            pStart += n;
            n = 0;
        }
    }
    if(n>0)submitHttpRequest(pStart, n);
    return 0;
}


void MM_TTSService::proc(void *ctx)
{
    MM_TTSService *handle = (MM_TTSService*)ctx;
    text_item_t txtInputItem;

    handle->m_bIsRunning =true;
    while(handle->m_bStopFlag!=true) {
        if (xQueueReceive(handle->m_cmdQueue, &txtInputItem, pdMS_TO_TICKS(1000))!=pdTRUE){
            continue;
        }
        ESP_LOGI(TAG, "Got text for TTS: %s", txtInputItem.ptr);
        ESP_LOGI(TAG, "Text length: %d", txtInputItem.len);
        //send one ring buffer play request per input text request(don't send one play request per split text piece. rb_done_write() does not work in that way).
        SoundPlayer::getInstance()->sendRingBufferHandle(handle->m_ringBufferHandle);
        //process input text
        handle->processTextInput(txtInputItem.ptr, txtInputItem.len);
        if(txtInputItem.freemem==true) {
            free(txtInputItem.ptr);
        }
        //if processTextInput() did not get any audio data from server, then rb_done_write will trigger an error on MP3_DECODER.
        //This is expected because MP3_DECODER is not able to read any data.
        rb_done_write(handle->m_ringBufferHandle);
        ESP_LOGW(TAG, "uxTaskGetStackHighWaterMark=%d bytes.", uxTaskGetStackHighWaterMark(NULL)*4);
    }

    handle->m_bIsRunning = false;
    vTaskDelete(NULL);
}

int MM_TTSService::start()
{
    connect(MM_URL_TPL);
    m_cmdQueue = xQueueCreate(4, sizeof(text_item_t));
    if(m_cmdQueue==NULL) {
        ESP_LOGE(TAG, "Failed to create queue.");
    }
    m_ringBufferHandle = rb_create(512, 32);
    if(m_ringBufferHandle==NULL) {
        ESP_LOGE(TAG, "Failed to create ring buffer.");
    }

    m_bStopFlag = false;
    xTaskCreate(MM_TTSService::proc, "tts_svc", 1024*4, (void*)this, 5, NULL);
    return 0;
}

int MM_TTSService::stop()
{
    m_bStopFlag = true;

    if(m_cmdQueue!=NULL) vQueueDelete(m_cmdQueue);
    m_cmdQueue = NULL;
    return ESP_OK;
}
