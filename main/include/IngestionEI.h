#pragma once

class IngestionEI {
private:
    void *m_httpClient;
    void *m_pJsonNodeRoot;

    char* createJsonStr(float(*data)[3], size_t len);
    int freeJsonStr(char *jsonStr);
    int sendData(const char *label, const char *jsonStr, size_t len);
public:
    int init();
    void deinit();
    int ingest(const char *label, float(*data)[3], size_t len);
};

