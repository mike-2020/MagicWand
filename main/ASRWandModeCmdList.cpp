#include "ASRTaskMgr.h"
#include "MagicWand.h"
#include "SoundPlayer.h"
#include "GestureCharmCommon.h"

#define ASR_WANDMODE_CMD_INDEX(X)         (X+CMD_INDEX_OFFSET_WANDMODE)
void task_onRecvCharm(int cmd_idx);

asr_task_cmd_t *asr_wand_mode_task_list = NULL;
static int asr_wand_mode_task_count = 0;

void task_onRecvCharm(int cmd_idx)
{
    const char *msg = asr_wand_mode_task_list[cmd_idx].cmd_msg;
    MagicWand::getInstance()->updateCharm(msg);
}

static void buildASRCmdList()
{
    asr_wand_mode_task_count = getCharmMsgCount();
   
    if(asr_wand_mode_task_list!=NULL) free(asr_wand_mode_task_list);
    asr_wand_mode_task_list = (asr_task_cmd_t*)malloc(asr_wand_mode_task_count*sizeof(asr_task_cmd_t));
   
    int i = 0, j = 0;
    const char *msg = NULL;
    while(getCharmMsg(i, &msg)==0) {
        i++;
        if(msg == NULL) continue;
        asr_wand_mode_task_list[j].cmd_msg = msg;
        asr_wand_mode_task_list[j].rsp_msg = NULL;
        asr_wand_mode_task_list[j].proc = task_onRecvCharm;
        j++;
    }
}


int ASRTaskMgr::updateWandModeCmdList()
{
    buildASRCmdList();

    esp_err_t rc = ESP_OK;
    for(int i = 0; i< asr_wand_mode_task_count; i++) {
        if(asr_wand_mode_task_list[i].cmd_msg==NULL) break;
        int cmdIdx = ASR_WANDMODE_CMD_INDEX(i);
        ASRTaskMgr::updateCmd(cmdIdx, asr_wand_mode_task_list[i].cmd_msg);
    }
    return rc;
}


