#include "esp_log.h"
#include "esp_system.h"
#include "MM_BaseService.h"
#include "MM_LLMService.h"
#include "MM_TTSService.h"

static const char *MM_APIKEY  = "";
static const char *MM_GROUPID = "";
static const char *TAG = "MM_BaseService";


static MM_LLMService g_llmService;
static MM_TTSService g_ttsService;

MM_BaseService::MM_BaseService()
{
    m_bStopFlag = true;
    m_bIsRunning = false;
}

int MM_BaseService::connect(const char *url)
{
    int rc = 0;
    char buffer[1024];

    if(m_httpClient!=NULL) {
        ESP_LOGE(TAG, "Re-initialize HTTP client before clean up it.");
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 1000*10,
        //.disable_auto_redirect = false,
        .max_redirection_count = 2,
        .event_handler = NULL,
        .buffer_size_tx = 1024,
        .user_data = NULL,        
        .keep_alive_enable = true,
        .keep_alive_interval = 5,
        .keep_alive_count = 5
    };
    m_httpClient = esp_http_client_init(&config);
    if(m_httpClient==NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client for URL %s.(free memory=%d bytes)", url, esp_get_free_heap_size());
        return ESP_FAIL;
    }

    sprintf(buffer, "%s?GroupId=%s", url, MM_GROUPID);
    esp_http_client_set_url(m_httpClient, buffer);
    esp_http_client_set_method(m_httpClient, HTTP_METHOD_POST);
    esp_http_client_set_header(m_httpClient, "Content-Type", "application/json");

    sprintf(buffer, "Bearer %s", MM_APIKEY);
    esp_http_client_set_header(m_httpClient, "Authorization", buffer);

    ESP_LOGI(TAG, "HTTP client has been initialized for URL %s.", url);
    return ESP_OK;
}


int MM_BaseService::doHttpRequest(char *reqMsg, int len)
{
    int rc = 0;

    ESP_LOGI(TAG, "Submit HTTP request for text: %s.", reqMsg);

    char *jsonReq = buildRequest(reqMsg, len);
    if(jsonReq==NULL) {
        ESP_LOGE(TAG, "Failed to build json request.");
        return ESP_FAIL;
    }
    size_t jsonReqLen = strlen(jsonReq);

    rc = esp_http_client_open(m_httpClient, jsonReqLen);
    if(rc!=ESP_OK) {
        ESP_LOGW(TAG, "Failed to open HTTP connection(rc=0x%x). min free heep=%d bytes, free internal heap=%d bytes.", rc, esp_get_minimum_free_heap_size(), esp_get_free_internal_heap_size());
        rc = -ESP_ERR_HTTP_EAGAIN;
        goto out;
    }
    rc = esp_http_client_write(m_httpClient, jsonReq, jsonReqLen);
    freeRequest(jsonReq);
    if(rc!=jsonReqLen) {
        ESP_LOGW(TAG, "Writing to remote HTTP endpoint error.");
        rc = -ESP_ERR_HTTP_EAGAIN;
        goto out;
    }
    rc = esp_http_client_fetch_headers(m_httpClient);
    if(rc<0) {
        ESP_LOGW(TAG, "Failed to fetch HTTP headers(rc=0x%x).", rc);
        rc = -ESP_ERR_HTTP_EAGAIN;
        goto out;
    }
    rc = esp_http_client_get_status_code(m_httpClient);
    ESP_LOGW(TAG, "HTTP response code: %d.", rc);
    if(rc==200) {
        int n = esp_http_client_get_content_length(m_httpClient);
        ESP_LOGI(TAG, "Content length: %d.", n);
        rc = processResponse(n);
    }else{
        rc = ESP_FAIL;
    }
out:
    esp_http_client_close(m_httpClient);
    return rc;
}

int MM_BaseService::submitHttpRequest(char *reqMsg, int len)
{
    int rc = 0;
    int n = 0;
    while(n++<3)
    {
        rc = doHttpRequest(reqMsg, len);
        if(rc==-ESP_ERR_HTTP_EAGAIN) {
            ESP_LOGW(TAG, "Retry HTTP request for %d times.", n);
            continue;
        }else break;
    }
    return rc;
}


int MM_BaseService::send(char *txt, bool bZeroCopy)
{
    text_item_t txtInputItem;

    if(m_cmdQueue==NULL) {
        ESP_LOGE(TAG, "Service has not been initialized.");
        return ESP_FAIL;
    }
    
    txtInputItem.len = strlen(txt);
    if(bZeroCopy==true) {
        txtInputItem.ptr = txt;
        txtInputItem.freemem = true;
    }else{
        txtInputItem.ptr = (char *)malloc(txtInputItem.len+1);
        if(txtInputItem.ptr==NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory(len=%d).", txtInputItem.len);
            return ESP_FAIL;
        }
        strcpy(txtInputItem.ptr, txt);
        txtInputItem.freemem = true;
    }
    BaseType_t rc = xQueueSend(m_cmdQueue, &txtInputItem, pdMS_TO_TICKS(200));
    if(rc!=pdTRUE) {
        ESP_LOGE(TAG, "Failed to send text for TTS.");
        return ESP_FAIL;
    }
    return ESP_OK;
}

MM_BaseService* MM_BaseService::getLLMInstance()
{
    return &g_llmService;
}

MM_BaseService* MM_BaseService::getTTSInstance()
{
    return &g_ttsService;
}
