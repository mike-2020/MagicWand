#pragma once

typedef struct{
    uint8_t cmd;
    char data[32];
    const char *strPtr;
}gesture_cmd_t;

typedef struct{
    const char *gestureLabel;
    const char *voiceLabel;
    const char *charmMsg;
    uint8_t state;
    int64_t lastStateUpdate;
    const char *musicLabel;
    uint16_t irmCode;
    uint16_t lightStyle;
}gesture_charm_t;



void reportGestureName(const char *name);
int verifyAndUpdateGesture(const char *name);
int queryActionsByCharm(const char *charmMsg, const char **musicLabel, uint16_t *irmCode, uint16_t *lightStyle);
int queryActionsByGesture(const char *gestureLabel, const char **musicLabel, uint16_t *irmCode, uint16_t *lightStyle);
int getCharmMsg(int idx, const char **msg);
int getCharmMsgCount();
