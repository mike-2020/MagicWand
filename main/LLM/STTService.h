#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ringbuf.h"

typedef struct {
    char *ptr;
    size_t len;
    uint16_t seq;
}audio_frame_t;

#define SEQ_LAST_NUM        0xFFFF
#define BUFFER_POOL_LEN     8
#define STT_ENC_RAW     1
#define STT_ENC_SPEEX   2

#define STT_SPEECH_SAMPLE           16000
#define STT_SPEECH_ENCODING         STT_ENC_SPEEX
#define STT_SPEECH_SPEEX_QUALITY    7

typedef struct {
    char *ptr;
    bool inUse;
}buffer_pool_item_t;

class STTService {
private:
    //buffer_pool_item_t m_ptrBufferPool[BUFFER_POOL_LEN];
    //int m_nBufferPoolItemCount;
    //SemaphoreHandle_t m_semBufferPool;
    char *m_pEncodingFrameBuffer;
    int m_nEncodingFrameLen;
    int m_nEncodingFrameOccupiedLen;
protected:
    bool m_bStopFlag;
    
protected:
    int init();
    int deinit();
    int encodeSpeex(char *pInBuffer, int nInLen, ringbuf_handle_t rawStream);
    void resetEncoding();
public:
    virtual int start() = 0;
    virtual int stop() = 0;
    //virtual int sendFrame(char *data, size_t len, uint16_t seq) = 0;
    virtual int captureSpeech(char *pBuffer, int len) = 0;
    static STTService *getInstance();
    int getFrameBuffer(char **pBuffer, size_t *len);
    int returnFrameBuffer(char *ptr);
    void initFrameBufferPool();
    void deinitFrameBufferPool();
    int getRawFrameLen() { return m_nEncodingFrameLen; };
};

class XF_STTService : public STTService {
private:
    //QueueHandle_t m_cmdQueue;
    ringbuf_handle_t m_ringBufferHandle;
    bool m_bIsDoneWrite;
    bool m_bPendingWsClose;
    int m_nWsState;
    char *m_rspText;
    char *m_svcURL;
    char *m_pAudioFrameBuffer;
    int m_nAudioFrameBufferLen;
    char *m_strSpeexParams;
    char *m_strEncoding;
    static void proc(void *ctx);
    static void wsEventHandler(void *handler_args, const char* base, int32_t event_id, void *event_data);
    int startWSClient();
    int buildURL(const char* secret, const char* key);
    void onRecvData(const char *data);
    int sendAudioFrame(const char *data, size_t len, uint16_t seq);
    int processAudioData();
public:
    XF_STTService();
    virtual int start();
    virtual int stop();
    //virtual int sendFrame(char *data, size_t len, uint16_t seq);
    virtual int captureSpeech(char *pBuffer, int len);
};
