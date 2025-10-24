#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "BD_BaseService.h"

static const char *TAG = "BD_BaseService";
static const char *BD_TOKEN_URL = "https://aip.baidubce.com/oauth/2.0/token";
static const char *BD_GRANT_TYPE= "client_credentials";
static const char *BD_CLIENT_ID = "";
static const char *BD_CLIENT_SECRET = "";

int BD_BaseService::connect(const char *url)
{
    int rc = 0;
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 1000*20,
        .disable_auto_redirect = false,
        .event_handler = NULL,
        .buffer_size_tx = 1024,
        .user_data = NULL,        // Pass address of local buffer to get response
        .keep_alive_enable = true,
        .keep_alive_interval = 5,
        .keep_alive_count = 5
    };
    m_httpClient = esp_http_client_init(&config);
    esp_http_client_set_url(m_httpClient, url);
    esp_http_client_set_method(m_httpClient, HTTP_METHOD_POST);
    esp_http_client_set_header(m_httpClient, "Content-Type", "application/json");
    esp_http_client_set_header(m_httpClient, "Accept", "application/json");
    return ESP_OK;
}


int BD_BaseService::processTokenResponse(int contentLen, char *token, int *tokenLen)
{
    int rc = 0;
    int len = 0;
    cJSON *pNodeToken = NULL;
    char *pStr = NULL;
    if(contentLen==-1) contentLen = 1024;
    char *buffer = (char*)malloc(contentLen);
    len = esp_http_client_read_response(m_httpClient, buffer, contentLen-1);
    if(len <0) {
        ESP_LOGE(TAG, "Failed to read http response: %d.", len);
    }
    buffer[len] = '\0';
    
    cJSON *pNodeRecvRoot= cJSON_ParseWithLength(buffer, len);
    if(pNodeRecvRoot==NULL) {
        ESP_LOGE(TAG, "Failed to parse result in json: %s", cJSON_GetErrorPtr());
        rc = ESP_FAIL;
        goto out;
    }
    pNodeToken    = cJSON_GetObjectItem(pNodeRecvRoot, "access_token");
    pStr = cJSON_GetStringValue(pNodeToken);
    if(pStr==NULL) {
        ESP_LOGE(TAG, "Failed to get access token from json response: %s", cJSON_GetErrorPtr());
    }else{
        printf(pStr);
        if(token!=NULL)strcpy(token, pStr);
        if(tokenLen!=NULL) *tokenLen = strlen(token);
    }
out:
    free(buffer);
    if(pNodeRecvRoot!=NULL)cJSON_Delete(pNodeRecvRoot);
    return 0;
}

int BD_BaseService::getAccessToken(char *token, int *len)
{
    int rc = 0;
    char url[256];
    sprintf(url, "%s?grant_type=%s&client_id=%s&client_secret=%s", BD_TOKEN_URL, BD_GRANT_TYPE, BD_CLIENT_ID, BD_CLIENT_SECRET);
    connect(url);
    rc = esp_http_client_open(m_httpClient, 0);
    if(rc!=ESP_OK) {
        ESP_LOGW(TAG, "Failed to open HTTP connection.");
        goto out;
    }
    rc = esp_http_client_fetch_headers(m_httpClient);
    if(rc<0) {
        ESP_LOGW(TAG, "Failed to fetch HTTP headers.");
        goto out;
    }
    rc = esp_http_client_get_status_code(m_httpClient);
    ESP_LOGW(TAG, "HTTP response code: %d.", rc);
    if(rc==200) {
        int n = esp_http_client_get_content_length(m_httpClient);
        ESP_LOGI(TAG, "Content length: %d.", n);
        processTokenResponse(n, token, len);
    }else{
        rc = ESP_FAIL;
    }

out:
    esp_http_client_close(m_httpClient);
    return rc;
}