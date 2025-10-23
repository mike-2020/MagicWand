#pragma once

#define AUDIO_VOLUME_MAX   100
#define AUDIO_VOLUME_MID    60
#define AUDIO_VOLUME_MIN    10

#define AUDIO_MIC_CHAN_NUM  2  //the number of channels for MIC. differencial design for 1 MIC means 2 here.

class AudioMgr{
public:
    int init(void);
    void deinit();
    int initStep2();
    void setVolume(int vol);
public:
    static AudioMgr* getInstance();
};

