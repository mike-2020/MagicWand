#pragma once
#include "esp_http_client.h"

class BD_BaseService
{
private:
    esp_http_client_handle_t m_httpClient;
private:
    int connect(const char *url);
    int processTokenResponse(int contentLen, char *token, int *tokenLen);
public:
    int getAccessToken(char *token, int *len);
};
