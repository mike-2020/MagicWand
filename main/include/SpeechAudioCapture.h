#pragma once

class SpeechAudioCapture 
{
private:
    bool m_bStopFlag;
    bool m_bIsRunning;
    bool m_bPauseCaptureFlag;
    static void proc(void *ctx);
public:
    static SpeechAudioCapture *getInstance();
    SpeechAudioCapture();
    int start();
    int stop();
    void pause() { m_bPauseCaptureFlag = true; };
    void resume() { m_bPauseCaptureFlag = false; };
};