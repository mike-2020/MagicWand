#pragma once




#ifdef __cplusplus
extern "C" {
#endif


void asr_enable_listen();

void asr_disable_listen();

void asr_set_voiceprint_registration_mode(int val);
int asr_get_voiceprint_registration_mode();


#ifdef __cplusplus
}
#endif //


class ASRService {
private:
    bool m_bASRListenFlag;
    bool m_bWakeCmdDetected;
    bool m_bVADDetected;
    void *m_afeData;
    int m_nWakeTimeOut;
    bool m_bInitialied;
    bool m_bForceCmdDetect;
    bool m_bLLMSpeechStopFlag;
    bool m_bLLMSpeechRunning;
    int m_nLLMSpeechStopDelayCount;
private:
    int setupRecordPipeline();
    int initModel();
    int llmSendSpeech(int vadState, char *audioData, int len);
    static void feedTask(void *ctx);
    static void detectTask(void *ctx);
public:
    ASRService() { m_bInitialied = false; }
    int init();
    void enable() { m_bASRListenFlag = true; }
    void disable() { m_bASRListenFlag = false; }
    void enableForceCmdDetect() { m_bForceCmdDetect = true; }
    void disableForceCmdDetect() { m_bForceCmdDetect = false; }
    void llmSpeechStart() { m_bLLMSpeechStopFlag = false; }
    void llmSpeechEnd() { m_bLLMSpeechStopFlag = true; }
    bool isInitialized() { return m_bInitialied; }
    static ASRService* getInstance();
};
