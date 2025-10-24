#include <string>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sntp.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include <mbedtls/md.h>
#include "esp_websocket_client.h"
#include "esp_event.h"
#include <ArduinoJson.h>
#include "STTService.h"
#include "MM_BaseService.h"

#define XF_SPEECH_FRAME_LEN     60  //for PCM format
#define AUDIO_FRAME_BASE64_LEN  ((XF_SPEECH_FRAME_LEN + 2)/3*4 + (XF_SPEECH_FRAME_LEN-1)/57)
#define XF_STT_HOST             "iat-api.xfyun.cn"
#define XF_STT_REQ              "/v2/iat"
#define DATE_URL_BUFFER_LEN     40
#define XF_APPID                ""
#define XF_APISecret            ""
#define XF_APIKey               ""

const char *TAG = "XF_STTService";

const char *json_audio_packet_tpl_start = "    {  \
        \"common\":{                        \
           \"app_id\":\"%s\"            \
        },                                  \
        \"business\":{                      \
            \"language\":\"zh_cn\",         \
            \"domain\":\"iat\",             \
            \"accent\":\"mandarin\",         \
            \"vad_eos\":10000,               \
            %s                               \
        },                                  \
        \"data\":{                          \
                \"status\":%d,               \
                \"format\":\"audio/L16;rate=16000\",    \
                \"encoding\":\"%s\",       \
                \"audio\":\"";
//const char *json_audio_packet_tpl_mid = "\"data\":{\"status\": %s, \"format\": \"audio/L16;rate=16000\",\"encoding\": \"%s\",\"audio\":\"";
const char *json_audio_packet_tpl_end = "{\"data\":{\"status\":2 }}";
const char *signature_tpl = "host: %s\ndate: %s\nGET %s HTTP/1.1";
const char *authorization_tpl = "api_key=\"%s\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"%s\"";
const char *url_tpl = "wss://%s%s?authorization=%s&date=%s&host=%s";

static XF_STTService g_xfSTTService;
static esp_websocket_client_handle_t g_wsClient;

int getDateTime(char *dateStr)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    time(&now);
    gmtime_r(&now, &timeinfo);
    strftime(dateStr, DATE_URL_BUFFER_LEN, "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
    return 0;
}

//处理url格式
void formatDateForURL(const char *dateStr, char *dateBuffer) 
{
    const char *p = dateStr;
    int i = 0;
    while(*p!='\0')
    {
        switch(*p) {
            case ' ': 
            {
                dateBuffer[i] = '+';
                i++;
                break;
            }
            case ',':
            {
                strcat(&dateBuffer[i], "%2C");
                i += 3;
                break;
            }
            case ':':
            {
                strcat(&dateBuffer[i], "%3A");
                i += 3;
                break;  
            }
            default:
            {
                dateBuffer[i] = *p;
                i++;
            }
        }
        if(i>=DATE_URL_BUFFER_LEN) {
            ESP_LOGE(TAG, "Date string URL encoding buffer overflow.");
            return;
        }
        dateBuffer[i] = '\0';
        p++;
    }
}


//构造讯飞ws连接url
int XF_STTService::buildURL(const char* secret, const char* key) 
{
    char buffer[256];
    char dateStr[DATE_URL_BUFFER_LEN]={0};
    getDateTime(dateStr);
    ESP_LOGI(TAG, "dateStr = %s", dateStr);
    sprintf(buffer, signature_tpl, XF_STT_HOST, dateStr, XF_STT_REQ);

    // 使用 mbedtls 计算 HMAC-SHA256
    unsigned char hmacResult[32];  // SHA256 产生的哈希结果长度为 32 字节
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);  // 1 表示 HMAC
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret, strlen(secret));
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)buffer, strlen(buffer));
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // 对结果进行 Base64 编码
    unsigned char base64Result[64];
    size_t base64ResultLen = 0;
    mbedtls_base64_encode(base64Result, 64, &base64ResultLen, hmacResult, 32);
    base64Result[base64ResultLen] = '\0';

    sprintf(buffer, authorization_tpl, key, base64Result);

    unsigned char authorization[256];
    size_t authorizationLen = 256;
    mbedtls_base64_encode(authorization, sizeof(authorization), &authorizationLen, (const unsigned char*)buffer, strlen(buffer));
    authorization[authorizationLen] = '\0';

    char dateURL[DATE_URL_BUFFER_LEN];
    formatDateForURL(dateStr, dateURL);
    sprintf(m_svcURL, url_tpl, XF_STT_HOST, XF_STT_REQ, authorization, dateURL, XF_STT_HOST);

    return 0;
}


void XF_STTService::wsEventHandler(void *handler_args, const char* base, int32_t event_id, void *event_data)
{
    XF_STTService *handle = (XF_STTService*)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    handle->m_nWsState = event_id;
    switch (event_id) {
        case WEBSOCKET_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
            break;
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED(HTTP Status Code=%d)", data->error_handle.esp_ws_handshake_status_code);
            break;
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
            ESP_LOGI(TAG, "Received opcode=%d", data->op_code);
            //if (data->op_code == 0x2) { // Opcode 0x2 indicates binary data
                //ESP_LOG_BUFFER_HEX("Received binary data", data->data_ptr, data->data_len);
            if (data->op_code == 0x08 && data->data_len == 2) {
                ESP_LOGW(TAG, "Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
            } else {
                ESP_LOGW(TAG, "Received=%.*s\n\n", data->data_len, (char *)data->data_ptr);
            }

            // If received data contains json structure it succeed to parse
            if(data->op_code == 0x01) {
                handle->onRecvData(data->data_ptr);
            }
            if(data->payload_len>0) {
                ESP_LOGW(TAG, "Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR(HTTP Status Code=%d)", data->error_handle.esp_ws_handshake_status_code);
            break;
        case WEBSOCKET_EVENT_CLOSED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
            break;
        }
}

int XF_STTService::startWSClient()
{
    esp_err_t rc = ESP_OK;

    //allocate websocket client handle
    if(g_wsClient==NULL) {
        esp_websocket_client_config_t websocket_cfg = {};

        websocket_cfg.host = XF_STT_HOST;
        websocket_cfg.skip_cert_common_name_check = true;
        websocket_cfg.keep_alive_enable = true;

        g_wsClient = esp_websocket_client_init(&websocket_cfg);
        esp_websocket_register_events(g_wsClient, WEBSOCKET_EVENT_ANY, XF_STTService::wsEventHandler, (void *)this);
    }

    //build URL and connect (URL buildout is always required because it contains datetime.)
    buildURL(XF_APISecret, XF_APIKey);
    esp_websocket_client_set_uri(g_wsClient, m_svcURL);
    ESP_LOGI(TAG, "Connecting to %s...", m_svcURL);
    int n = 0;
    while(n++<3) {
        rc = esp_websocket_client_start(g_wsClient);
        if(rc==ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "Retry to connect to server for %d times...", n);
    }
    if(rc!=ESP_OK)ESP_LOGE(TAG, "Failed to start websocket client: %d.", rc);
    return rc;
}

int XF_STTService::sendAudioFrame(const char *data, size_t len, uint16_t seq)
{
    int rc = 0;

    if(m_bPendingWsClose==true) {
        esp_websocket_client_close(g_wsClient, portMAX_DELAY);
        m_bPendingWsClose = false;
    }
    //check if reconnect is required.
    if(m_nWsState==WEBSOCKET_EVENT_CLOSED) {
        rc = startWSClient();
        if(rc!=ESP_OK) return rc;
    }

    //for the last audio frame
    if(seq==SEQ_LAST_NUM) {
        rc = esp_websocket_client_send_text(g_wsClient, json_audio_packet_tpl_end, strlen(json_audio_packet_tpl_end), portMAX_DELAY);
        return rc;
    }

    //for the first audio frame
    if(seq==0) {
        m_rspText[0] = '\0';    //clear response text
        sprintf(m_pAudioFrameBuffer, json_audio_packet_tpl_start, XF_APPID, m_strSpeexParams, 0, m_strEncoding);
    //not the first or last audio frame
    }else{
        sprintf(m_pAudioFrameBuffer, json_audio_packet_tpl_start, XF_APPID, m_strSpeexParams, 1, m_strEncoding);
    }

    int bufferUsedLen = strlen(m_pAudioFrameBuffer);
    char *p = m_pAudioFrameBuffer + bufferUsedLen;
    size_t base64AudioLen = 0;
    rc = mbedtls_base64_encode((unsigned char*)p, m_nAudioFrameBufferLen - bufferUsedLen, &base64AudioLen, (const unsigned char*)data, len);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to do base64 encoding on audio frame.(bufferUsedLen=%d, bufferFreeLen=%d)", bufferUsedLen, m_nAudioFrameBufferLen - bufferUsedLen);
        return rc;
    }
    p[base64AudioLen] = '\0';
    strcat(m_pAudioFrameBuffer, "\"}}");    //enclose the string to make it a JSON object
    
    rc = esp_websocket_client_send_text(g_wsClient, m_pAudioFrameBuffer, strlen(m_pAudioFrameBuffer), portMAX_DELAY);
    //ESP_LOGI(TAG, "%s", m_pAudioFrameBuffer);
    return rc;
}

void XF_STTService::onRecvData(const char *data)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);
    if (error) {
      ESP_LOGW(TAG, "Failed to parse JSON: %s", error.c_str());
      return;
    }

    if(doc["code"]!=0) {
        ESP_LOGE(TAG, "XF server return error message: %s.", doc["message"]);
        return;
    }

    JsonArray ws = doc["data"]["result"]["ws"];
    for (JsonObject word : ws) {
      int bg = word["bg"];
      const char* w = word["cw"][0]["w"];
      strcat(m_rspText, w);
    }
    if (doc["data"]["status"] == 2) {  //收到结束标志
        //sttste = 1;
        ESP_LOGI(TAG, "Response text: %s.", m_rspText);
        MM_BaseService::getLLMInstance()->send(m_rspText);
        m_bPendingWsClose = true;
    }
}
#if 0
void XF_STTService::proc(void *ctx)
{
    XF_STTService *handle = (XF_STTService*)ctx;
    audio_frame_t audioFrame;
    uint16_t nLastSeq = SEQ_LAST_NUM;
    while(1) {
        if (xQueueReceive(handle->m_cmdQueue, &audioFrame, pdMS_TO_TICKS(1000))!=pdTRUE){
            if(nLastSeq!=SEQ_LAST_NUM) { //timeout
                ESP_LOGW(TAG, "audio frame processing timeout: %d.", nLastSeq);
            }
            continue;
        }
        handle->sendAudioFrame(audioFrame.ptr, audioFrame.len, audioFrame.seq);
        if(audioFrame.ptr!=NULL) {
            handle->returnFrameBuffer(audioFrame.ptr);
        }
        nLastSeq = audioFrame.seq;
    }
}
#endif

int XF_STTService::processAudioData()
{
    char buf[XF_SPEECH_FRAME_LEN];
    int len = 0;
    static uint16_t seq = 0;
    len = rb_read(m_ringBufferHandle, buf, sizeof(buf), pdMS_TO_TICKS(100));
    if(len == RB_TIMEOUT) return ESP_OK;
    //something wrong
    if(len!=RB_DONE && len<0) {
        ESP_LOGE(TAG, "Ring buffer error: %d.", len);
        return ESP_FAIL;
    }
    //got speech end signal
    if(len==RB_DONE) {
        ESP_LOGI(TAG, "processAudioData: Got speech end signal.");
        seq = SEQ_LAST_NUM;
        sendAudioFrame(NULL, 0, seq);
        rb_reset_is_done_write(m_ringBufferHandle);
        m_bIsDoneWrite = false;
        ESP_LOGW(TAG, "uxTaskGetStackHighWaterMark=%d bytes.", uxTaskGetStackHighWaterMark(NULL)*4);
    //normal speech data
    }else if(len >=0) {
        sendAudioFrame(buf, len, seq);
    }
    ESP_LOGI(TAG, "Sent frame, len = %d, seq = %d.", len, seq);

    if(seq==SEQ_LAST_NUM) {
        seq = 0;
    }else{
        seq++;
    }
    return ESP_OK;
}

void XF_STTService::proc(void *ctx)
{
    XF_STTService *handle = (XF_STTService*)ctx;
    while(handle->m_bStopFlag != true) {
        handle->processAudioData();
        if(handle->m_bPendingWsClose==true) {
            esp_websocket_client_close(g_wsClient, portMAX_DELAY);
            handle->m_bPendingWsClose = false;
        }
    }
}

XF_STTService::XF_STTService()
{
    m_ringBufferHandle = NULL;
    m_bIsDoneWrite = false;
    m_bPendingWsClose = false;
    m_nWsState==WEBSOCKET_EVENT_CLOSED;
    if(STT_SPEECH_ENCODING==STT_ENC_SPEEX) {
        m_strEncoding = "speex-wb";
        m_strSpeexParams = "\"speex_size\" : 60";
    }else{
        m_strEncoding = "raw";
        m_strSpeexParams = "";
    }
}

int XF_STTService::start()
{
    m_rspText = (char*) malloc(512);
    m_svcURL = (char*) malloc(512);
    m_nAudioFrameBufferLen = AUDIO_FRAME_BASE64_LEN + 1024;
    m_pAudioFrameBuffer = (char*)malloc(m_nAudioFrameBufferLen);
    if(m_rspText==NULL || m_svcURL==NULL || m_pAudioFrameBuffer==NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory.");
        return ESP_FAIL;
    }

    m_ringBufferHandle = rb_create(1024, 64);
    if(m_ringBufferHandle==NULL) {
        ESP_LOGE(TAG, "Failed to allocated ring buffer.");
    }
    //m_cmdQueue = xQueueCreate(4, sizeof(audio_frame_t));
    startWSClient();    //connect websocket server
    STTService::init();

    m_bStopFlag = false;
    xTaskCreate(XF_STTService::proc, "sttservice", 1024*6, (void*)this, 5, NULL);
    return 0;
}

int XF_STTService::stop()
{
    m_bStopFlag = true;
    return 0;
}

STTService *STTService::getInstance()
{
    return &g_xfSTTService;
}


int XF_STTService::captureSpeech(char *pBuffer, int len)
{
    int rc = 0;

    if(m_ringBufferHandle==NULL){
        ESP_LOGE(TAG, "Invalid ring buffer handle.");
        return ESP_FAIL;
    }

    if(pBuffer==NULL){
        ESP_LOGI(TAG, "sendAudioData: Got speech end signal.");
        rb_done_write(m_ringBufferHandle);
        m_bIsDoneWrite = true;
        resetEncoding();
        return 0;
    }
    //not process new coming data until all data in the pipeline has been consumed 
    if(m_bIsDoneWrite==true) {
        ESP_LOGI(TAG, "m_bIsDoneWrite==true");
        return 0;
    }

    return encodeSpeex(pBuffer, len, m_ringBufferHandle);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
int STTService::getFrameBuffer(char **ptr, size_t *len)
{
    int rc = ESP_FAIL;
    xSemaphoreTake(m_semBufferPool, portMAX_DELAY);

    for(int i=0; i<BUFFER_POOL_LEN; i++){
        if(m_ptrBufferPool[i].inUse!=true && m_ptrBufferPool[i].ptr!=NULL) {
            *ptr = m_ptrBufferPool[i].ptr;
            m_ptrBufferPool[i].inUse = true;
            *len = XF_SPEECH_FRAME_LEN;
            xSemaphoreGive(m_semBufferPool);
            return ESP_OK;
        }
    }
    if(m_nBufferPoolItemCount > BUFFER_POOL_LEN) {
        xSemaphoreGive(m_semBufferPool);
        ESP_LOGW(TAG, "Used frame buffer items reached limit.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Allocate new frame buffer.");
    char *p = (char *)malloc(XF_SPEECH_FRAME_LEN);
    if(p == NULL) {
        xSemaphoreGive(m_semBufferPool);
        ESP_LOGE(TAG, "Failed to allocated memory.");
        return ESP_FAIL;
    }
    for(int i=0; i<BUFFER_POOL_LEN; i++){
        if(m_ptrBufferPool[i].ptr==NULL) {
            m_ptrBufferPool[i].ptr = p;
            m_ptrBufferPool[i].inUse = true;
            m_nBufferPoolItemCount++;
            *ptr = p;
            *len = XF_SPEECH_FRAME_LEN;
            xSemaphoreGive(m_semBufferPool);
            return ESP_OK;
        }
    }
    xSemaphoreGive(m_semBufferPool);
    return ESP_FAIL;
}

int STTService::returnFrameBuffer(char *ptr)
{
    int rc = ESP_FAIL;
    ESP_LOGI(TAG, "Frame buffer returned: ptr = %p.", ptr);
    xSemaphoreTake(m_semBufferPool, portMAX_DELAY);
    for(int i=0; i<BUFFER_POOL_LEN; i++){
        if(m_ptrBufferPool[i].ptr==ptr) {
            m_ptrBufferPool[i].inUse = false;
            rc = ESP_OK;
            break;
        }
    }
    xSemaphoreGive(m_semBufferPool);
    return rc;
}

void STTService::initFrameBufferPool()
{
    for(int i=0; i<BUFFER_POOL_LEN; i++){
        m_ptrBufferPool[i].ptr = NULL;
        m_ptrBufferPool[i].inUse = false;
    }
    m_nBufferPoolItemCount = 0;
    m_semBufferPool = xSemaphoreCreateMutex();
}

void STTService::deinitFrameBufferPool()
{
    for(int i=0; i<BUFFER_POOL_LEN; i++){
        if(m_ptrBufferPool[i].ptr != NULL) {
            free(m_ptrBufferPool[i].ptr);
            m_ptrBufferPool[i].ptr = NULL;
            m_ptrBufferPool[i].inUse = false;
        }
    }
    m_nBufferPoolItemCount = 0;
    vSemaphoreDelete(m_semBufferPool);
}

int XF_STTService::sendFrame(char *data, size_t len, uint16_t seq)
{
    audio_frame_t frame;
    frame.ptr = data;
    frame.len = len;
    frame.seq = seq;

    BaseType_t rc = xQueueSend(m_cmdQueue, &frame, pdMS_TO_TICKS(200));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send frame: %u.", seq);
        returnFrameBuffer(data);
        return ESP_FAIL;
    }
    return ESP_OK;
}
#endif