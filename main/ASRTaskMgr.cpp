#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mn_speech_commands.h"
#include "ASRTaskMgr.h"
#include "AudioMgr.h"
#include "esp_log.h"
#include "ASRService.h"
#include "SoundPlayer.h"
#include "HMIMgr.h"
#include "PowerMgr.h"

static const char *TAG = "ASRTaskMgr";
extern asr_task_cmd_t asr_training_mode_task_list[];
extern asr_task_cmd_t asr_common_task_list[];
extern asr_task_cmd_t *asr_wand_mode_task_list;

int ASRTaskMgr::updateCmd(int cmdIdx, const char *msg)
{
    char buffer[64];
    const char *p, *q = msg;
    do{
        p = strchr(q, ',');
        if(p==NULL) {
            esp_mn_commands_add(cmdIdx, (char*)q);
            break;
        }else{
            memcpy(buffer, q, p - q);
            buffer[p - q] = '\0';
            esp_mn_commands_add(cmdIdx, buffer);
            q = p + 1;
        }
    }while(*p!='\0' && p < msg + strlen(msg));
    return 0;
}

void ASRTaskMgr::updateCmdList(int workingMode)
{
    //if(ASRService::getInstance()->isInitialized()==false) return;

    esp_mn_commands_clear();
    updateCommonCmdList();
    if(workingMode==WORKING_MODE_TRAINING) {
        updateTrainingModeCmdList();
    }else if(workingMode==WORKING_MODE_WAND){
        updateWandModeCmdList();
    }
    esp_mn_commands_update();
    esp_mn_active_commands_print();
}

int ASRTaskMgr::onCmdReceived(int cmd_idx)
{
    if(cmd_idx==ASR_CMD_EXIT_WAKEUP) {
        return ASR_CMD_EXIT_WAKEUP;
    }

    PowerMgr::getInstance()->updateInputActivityTime();

    asr_task_cmd_t *pTaskNode = NULL;
    if(cmd_idx < CMD_INDEX_OFFSET_TRAINING) {                              //for common mode
        pTaskNode = &asr_common_task_list[cmd_idx];
    }else if(cmd_idx >= CMD_INDEX_OFFSET_TRAINING && cmd_idx < CMD_INDEX_OFFSET_WANDMODE){        //for training mode
        cmd_idx = cmd_idx - CMD_INDEX_OFFSET_TRAINING;
        pTaskNode = &asr_training_mode_task_list[cmd_idx];
    }else if(cmd_idx >= CMD_INDEX_OFFSET_WANDMODE){      //for unknown mode
        cmd_idx = cmd_idx - CMD_INDEX_OFFSET_WANDMODE;
        pTaskNode = &asr_wand_mode_task_list[cmd_idx];
    }

    if(pTaskNode->proc!=NULL) pTaskNode->proc(cmd_idx);
    if(pTaskNode->rsp_msg!=NULL) {
        SoundPlayer::getInstance()->speak(pTaskNode->rsp_msg);
    }
    return 0;
}

int ASRTaskMgr::onWakeup()
{
    SoundPlayer::getInstance()->speak(VOICE_WAKEUP);
    return 0;
}

static ASRTaskMgr g_asrTaskMgr;
ASRTaskMgr* ASRTaskMgr::getInstance()
{
    return &g_asrTaskMgr;
}