#pragma once
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ringbuf.h"
#include "audio_element.h"


enum soundplayer_mode_t{
    SOUNDPLAYER_MODE_NONE = 0,
    SOUNDPLAYER_MODE_MP3FILE = 1,
    SOUNDPLAYER_MODE_MP3TTS
};

typedef struct {
    char str[64];
    void *ptr;
    soundplayer_mode_t mode;
    TaskHandle_t task;
}sound_cmd_t;

class SoundPlayer{
private:
    QueueHandle_t m_cmdQueue;
    void *m_audioPipeline;
    int m_nVolume;
    FILE *m_curFileHandle;
    char *m_nextFileNamePtr;
    ringbuf_handle_t m_curRingBufHandle;
    size_t m_curRawDataLen;
    size_t m_curRawDataPos;
    sound_cmd_t m_curCmd;

    int processNextCmd();
    static void proc(void *ctx);
    static int mp3DecoderReadCB(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx);
    FILE * openNextFile();
public:
    SoundPlayer();
    int init();
    void play(const char* str, bool bSyncFlag=false);
    void speak(const char* msg, bool bSyncFlag=false);
    void speakEx(const char* str, bool dup);
    void speakDigit(int num);
    void speakRspCode(int num);
    int buildNum(char *buff, int num);
    int speakWithNum(const char *name1, int num, const char *name2=NULL);
    void playEffect(int num=-1);  //by default, play sound effects randomly
    void playEffect(const char *name);
    ringbuf_handle_t openRingBuffer(int block_size, int n_blocks);
    int sendRingBufferHandle(ringbuf_handle_t handle);
    int stopPlay();
    static SoundPlayer* getInstance();
};


#define VOICE_SAMPLE_START      "SP-START"
#define VOICE_SAMPLE_END        "SP-END"
#define VOICE_SAMPLE_HELP       "SP-HELP"
#define VOICE_OK                "OK"
#define VOICE_WAKEUP            "WAKEUP"
#define VOICE_SAMPLE_PREP       "SP-PREP"
#define VOICE_SAMPLE_TMOUT      "SP-TMOUT"

#define VOICE_SHAPE_CIRCLE      "S-CIRCLE"
#define VOICE_SHAPE_CHECK       "S-CHECK"
#define VOICE_SHAPE_SQUARE      "S-SQUARE"
#define VOICE_SHAPE_TRANGLE     "S-TRIANG"
#define VOICE_SHAPE_CROSS       "S-CROSS"
#define VOICE_SHAPE_IDLE        "S-IDLE"
#define VOICE_SHAPE_W           "S-W"
#define VOICE_SHAPE_H           "S-H"
#define VOICE_SHAPE_R           "S-R"
#define VOICE_SHAPE_FI          "S-FI"
#define VOICE_SHAPE_UPDOWN      "S-UPDOWN"
#define VOICE_SHAPE_LEFTRIGHT   "S-LFTRGT"
#define VOICE_SHAPE_UNKNOWN     "S-UNKNOW"

#define VOICE_TRN_DATA_LESS     "T-DATLES"
#define VOICE_TRN_CUR_SHAPE     "T-CURSHP"
#define VOICE_TRN_SUBMIT_SUCCESS "T-CMTNDT"
#define VOICE_TRN_SUBMIT_FAIL   "T-DATFAL"



#define VOICE_RSCODE            "A-RSCODE"
#define VOICE_CONNECT_WIFI      "A-CNWIFI"
#define VOICE_WIFI_FAIL         "A-WIFIER"
#define VOICE_WIFI_SUCCESS      "A-WIFISU"

#define VOICE_UNKNOWN_GESTURE1  "A-UNGST1"
#define VOICE_UNKNOWN_GESTURE2  "A-UNGST2"
#define VOICE_NO_GESTURE        "A-NOGST"
#define VOICE_NEED_CHARM        "A-NECHRM"
#define VOICE_CHARM_ERROR       "A-CHRMER"

#define VOICE_EFFECT_RANDOM     ":SE:"