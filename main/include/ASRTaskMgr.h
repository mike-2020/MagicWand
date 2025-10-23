#pragma once
#include "HMIMgr.h"

typedef void(*asr_task_proc_t)(int);

typedef struct{
    const char *cmd_msg;
    const char *rsp_msg;
    asr_task_proc_t proc;
}asr_task_cmd_t;

#define ASR_TASK_EXIT_WAKEUP()  ASRTaskMgr::getInstance()->onCmdReceived(ASR_CMD_EXIT_WAKEUP) 
#define ASR_CMD_ID_START    1
#define ASR_CMD_EXIT_WAKEUP 1

#define CMD_INDEX_OFFSET_TRAINING   1000
#define CMD_INDEX_OFFSET_WANDMODE   2000


class ASRTaskMgr {
private:
    int updateCmd(int cmdIdx, const char *msg);
    int updateCommonCmdList();
    int updateTrainingModeCmdList();
    int updateWandModeCmdList();
public:
    int onCmdReceived(int cmd_idx);
    int onWakeup();
    void updateCmdList(int workingMode = WORKING_MODE_WAND);
    static ASRTaskMgr* getInstance();
};
