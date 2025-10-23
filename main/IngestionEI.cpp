#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "IngestionEI.h"
#include "SoundPlayer.h"


#define REMOTE_ENDPOINT         "https://ingestion.edgeimpulse.com"
#define API_KEY                 "ei_64c442c1ea58e51a0c28ad2c3e4f5c927e378cb128941f00032101648cd7a5f9"
#define REMOTE_URI_TRAINING     REMOTE_ENDPOINT"/api/training/data"

static const char *TAG = "IngestionEI";



int IngestionEI::init()
{
    esp_http_client_config_t config = {
        .url = REMOTE_ENDPOINT,
        .path = REMOTE_URI_TRAINING,
        .cert_pem = NULL,
        .timeout_ms = 1000*20,
        .disable_auto_redirect = false,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    m_httpClient = (void*)client;
    return 0;
}

void IngestionEI::deinit()
{
    esp_http_client_handle_t client = (esp_http_client_handle_t) m_httpClient;
    esp_http_client_cleanup(client);
}

int IngestionEI::ingest(const char *label, float(*data)[3] , size_t len)
{
    int rc = 0;
    char *jsonStr = createJsonStr(data, len);
    //printf(jsonStr);
    rc = sendData(label, jsonStr, strlen(jsonStr));
    freeJsonStr(jsonStr);
    if(rc!=ESP_OK) {
        ESP_LOGE(TAG, "Failed to ingest data: %d.", rc);
        return rc;
    }
    return 0;
}

char* IngestionEI::createJsonStr(float(*data)[3], size_t len)
{
    cJSON *pNodeRoot = cJSON_CreateObject();                         // 创建JSON根部结构体
    cJSON *pNodePayload = cJSON_CreateObject();  


    //for "sensors" block
    cJSON *pNodeSensors = cJSON_CreateArray(); 
    cJSON *pNodeSensor = cJSON_CreateObject();  
    cJSON_AddStringToObject(pNodeSensor,"name","accX");
    cJSON_AddStringToObject(pNodeSensor,"units","m/s2");
    cJSON_AddItemToArray(pNodeSensors, pNodeSensor);  
    pNodeSensor = cJSON_CreateObject();
    cJSON_AddStringToObject(pNodeSensor,"name","accY");
    cJSON_AddStringToObject(pNodeSensor,"units","m/s2");
    cJSON_AddItemToArray(pNodeSensors, pNodeSensor);    
    pNodeSensor = cJSON_CreateObject();
    cJSON_AddStringToObject(pNodeSensor,"name","accZ");
    cJSON_AddStringToObject(pNodeSensor,"units","m/s2");
    cJSON_AddItemToArray(pNodeSensors, pNodeSensor);   
    cJSON_AddItemToObject(pNodePayload, "sensors", pNodeSensors);

    //for "protected" block
    cJSON *pNodeProtected = cJSON_CreateObject();
    cJSON_AddStringToObject(pNodeProtected,"ver","v1");
    cJSON_AddStringToObject(pNodeProtected,"alg","none");
    cJSON_AddItemToObject(pNodeRoot, "protected", pNodeProtected);

    //for "payload" block
    cJSON_AddStringToObject(pNodePayload,"device_name","65:c6:3a:b2:33:c8");    // 添加字符串类型数据到根部结构体
    cJSON_AddStringToObject(pNodePayload,"device_type","ESP32-S3"); 
    cJSON_AddNumberToObject(pNodePayload,"interval_ms",10);                   // 添加整型数据到根部结构体

    cJSON *pNodePayloadValues = cJSON_CreateArray(); 
    cJSON *pNodePayloadValue = NULL;
    for(int i=0; i<len; i++) {
        pNodePayloadValue = cJSON_CreateFloatArray(data[i], 3); 
        cJSON_AddItemToArray(pNodePayloadValues, pNodePayloadValue);  
    }
    cJSON_AddItemToObject(pNodePayload, "values", pNodePayloadValues);

    cJSON_AddStringToObject(pNodeRoot,"signature","00");
    cJSON_AddItemToObject(pNodeRoot, "payload", pNodePayload);

    m_pJsonNodeRoot = (void *)pNodeRoot;
    return cJSON_Print(pNodeRoot);

}

int IngestionEI::freeJsonStr(char *jsonStr)
{
    cJSON_free((void *) jsonStr);                             // 释放cJSON_Print ()分配出来的内存空间
    cJSON_Delete((cJSON *)m_pJsonNodeRoot);                                       // 释放cJSON_CreateObject ()分配出来的内存空间
    return 0;
}

int IngestionEI::sendData(const char *label, const char *jsonStr, size_t len)
{
    int rc = 0;

    esp_http_client_handle_t client = (esp_http_client_handle_t) m_httpClient;
    esp_http_client_set_url(client, REMOTE_URI_TRAINING);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "x-api-key", API_KEY);
    esp_http_client_set_header(client, "x-label", label);
    esp_http_client_set_header(client, "X-File-Name", label);
    esp_http_client_set_header(client, "x-disallow-duplicates", "true");
    esp_http_client_set_header(client, "x-add-date-id", "1");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    rc = esp_http_client_open(client, len);
    if(rc!=ESP_OK) {
        ESP_LOGW(TAG, "Failed to open HTTP connection.");
        SoundPlayer::getInstance()->speakRspCode(rc);
        goto out;
    }
    rc = esp_http_client_write(client, jsonStr, len);
    if(rc!=len) {
        ESP_LOGW(TAG, "Writing to remote HTTP endpoint error.");
        SoundPlayer::getInstance()->speakRspCode(rc);
        rc = ESP_FAIL;
        goto out;
    }
    rc = esp_http_client_fetch_headers(client);
    if(rc<0) {
        ESP_LOGW(TAG, "Failed to fetch HTTP headers.");
        SoundPlayer::getInstance()->speakRspCode(rc);
        goto out;
    }
    rc = esp_http_client_get_status_code(client);
    if(rc!=200) {
        ESP_LOGW(TAG, "HTTP response: %d.", rc);
        SoundPlayer::getInstance()->speakRspCode(rc);
        #if 0
        int n = esp_http_client_get_content_length(client);
        char buffer[256];
        if(n>255) n = 255;
        esp_http_client_read_response(client, buffer, n);
        buffer[n] = '\0';
        printf(buffer);
        #endif
        rc = ESP_FAIL;
    }else{
        rc = ESP_OK;
    }
out:
    esp_http_client_close(client);

    return rc;
}