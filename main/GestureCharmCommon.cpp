#include "string.h"
#include <map>
#include "esp_log.h"
#include "esp_timer.h"
#include "SoundPlayer.h"
#include "GestureCharmCommon.h"
#include "IRMTransceiver.h"

static const char *TAG = "GestureShapeCommon";
#define GESTURE_CHARM_LIST_NUM()        (sizeof(gesture_charm_list)/sizeof(gesture_charm_t))
#define GC_STATE_GESTURE(X)              (X | 0x01)
#define GC_IS_GESTURE_UPDATED(X)        (X & 0x01)
#define GC_STATE_CHARM(X)              (X | 0x02)

static gesture_charm_t gesture_charm_list[] = {
    { "idle",     VOICE_SHAPE_IDLE, NULL, 0, 0, NULL, 0, 0 },
    { "circle",     VOICE_SHAPE_CIRCLE, "da kai dian shi,guan bi dian shi", 0, 0, VOICE_EFFECT_RANDOM, IRM_TV_ON, 0 },
    { "circle",     VOICE_SHAPE_CIRCLE, "guan deng", 0, 0, VOICE_EFFECT_RANDOM, IRM_LED_OFF, 0 },
    { "check",      VOICE_SHAPE_CHECK , NULL, 0, 0, NULL, 0, 0   },
    { "square",     VOICE_SHAPE_SQUARE, NULL, 0, 0, NULL, 0, 0   },
    { "triangle",    VOICE_SHAPE_TRANGLE, NULL, 0,0, NULL, 0, 0   },
    { "cross",      VOICE_SHAPE_CROSS, NULL, 0,  0, NULL, 0, 0   },
    { "letter-w",   VOICE_SHAPE_W, NULL, 0,  0, NULL, 0, 0   },
    { "letter-h",   VOICE_SHAPE_H, NULL, 0,  0, NULL, 0, 0   },
    { "letter-r",   VOICE_SHAPE_R, NULL, 0,  0, NULL, 0, 0   },
    { "left_right", VOICE_SHAPE_LEFTRIGHT, NULL, 0,  0, VOICE_EFFECT_RANDOM, IRM_TV_ON, 0   },
    { "up_down", VOICE_SHAPE_UPDOWN, "da kai dian shi,guan bi dian shi", 0,  0, VOICE_EFFECT_RANDOM, IRM_TV_ON, 0   },
};




void reportGestureName(const char *name)
{
    SoundPlayer::getInstance()->speak(VOICE_TRN_CUR_SHAPE);
    ESP_LOGI(TAG, "name=%s", name);
    for(int i = 0; i<GESTURE_CHARM_LIST_NUM(); i++) {
        if(strcmp(gesture_charm_list[i].gestureLabel, name)==0) {
            SoundPlayer::getInstance()->speak(gesture_charm_list[i].voiceLabel);
            return;
        }
    }
    SoundPlayer::getInstance()->speak(VOICE_SHAPE_UNKNOWN);
}

int verifyAndUpdateGesture(const char *name)
{
    for(int i=0; i<GESTURE_CHARM_LIST_NUM(); i++)
    {
        gesture_charm_t *gc = &gesture_charm_list[i];
        if(strcmp(gc->gestureLabel, name)==0) {
            gc->state = 1; //GC_STATE_GESTURE(gc->state);
            gc->lastStateUpdate = esp_timer_get_time();
            ESP_LOGI(TAG, "%s has been verified and updated at index %d.", name, i);
            return i;
        }
    }
    return -1;
}

int queryActionsByCharm(const char *charmMsg, const char **musicLabel, uint16_t *irmCode, uint16_t *lightStyle)
{
    int rc = 0;
    for(int i=0; i<GESTURE_CHARM_LIST_NUM(); i++)
    {
        gesture_charm_t *gc = &gesture_charm_list[i];
        if(gc->charmMsg!=NULL && strcmp(gc->charmMsg, charmMsg)==0) {
            ESP_LOGI(TAG, "Found charm %s, state = 0x%X, last update = %lld, at index %d.", charmMsg, gc->state, gc->lastStateUpdate, i);
            if(GC_IS_GESTURE_UPDATED(gc->state) || gc->gestureLabel == NULL) {
                int64_t tm = esp_timer_get_time();
                //ESP_LOGI(TAG, "tm = %lld, tm - gc->lastStateUpdate=%lld", tm, tm - gc->lastStateUpdate);
                if(tm - gc->lastStateUpdate < 15 * 1000 * 1000 || gc->gestureLabel == NULL){  //valid state, return actions
                    *musicLabel = gc->musicLabel;
                    *irmCode    = gc->irmCode;
                    *lightStyle = gc->lightStyle;
                    rc = i;
                }else{
                    rc = -1;
                }
                //clean the state no matter it is expired or not
                gc->state = 0;
                gc->lastStateUpdate = 0;
                return rc;
            }
        }
    }
    return -1;
}

int queryActionsByGesture(const char *gestureLabel, const char **musicLabel, uint16_t *irmCode, uint16_t *lightStyle)
{
    int rc = 0;
    for(int i=0; i<GESTURE_CHARM_LIST_NUM(); i++)
    {
        gesture_charm_t *gc = &gesture_charm_list[i];
        if(strcmp(gc->gestureLabel, gestureLabel)==0) {
            if(gc->charmMsg==NULL) {
                *musicLabel = gc->musicLabel;
                *irmCode    = gc->irmCode;
                *lightStyle = gc->lightStyle;
                ESP_LOGI(TAG, "queryActionsByGesture: irmCode=0x%X, lightStyle=0x%X.", gc->irmCode, gc->lightStyle);
                return i;
            }
        }
    }
    return -1;
}

int getCharmMsg(int idx, const char **msg)
{
    if(idx<0 || idx >=GESTURE_CHARM_LIST_NUM()) return -1;

    gesture_charm_t *gc = &gesture_charm_list[idx];
    if(gc->charmMsg!=NULL && strlen(gc->charmMsg)>0) {
        *msg = gc->charmMsg;
    }else{
        *msg = NULL;
    }
    return 0;
}

int getCharmMsgCount()
{
    int n = 0;
    for(int i=0; i<GESTURE_CHARM_LIST_NUM(); i++) {
        gesture_charm_t *gc = &gesture_charm_list[i];
        if(gc->charmMsg!=NULL && strlen(gc->charmMsg)>0) {
            n++;
        }
    }
    return n;
}