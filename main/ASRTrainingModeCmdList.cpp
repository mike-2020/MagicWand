#include "esp_mn_speech_commands.h"
#include "ASRTaskMgr.h"
#include "SampleRecorder.h"
#include "SoundPlayer.h"

#define ASR_TRAININGMODE_CMD_INDEX(X)         (X+CMD_INDEX_OFFSET_TRAINING)
#define ASR_TRAININGMODE_CMD_NUM()            (sizeof(asr_training_mode_task_list)/sizeof(asr_task_cmd_t))

void task_setTrainPlusShape(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("plus");
}

void task_setTrainCrossShape(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("cross");
}

void task_setTrainCheckShape(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("check");
}

void task_setTrainSquareShape(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("square");
}

void task_setTrainCircleShape(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("circle");
}

void task_setTrainGreatMarkShape(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("great");
}

void task_setTrainLessMarkShape(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("less");
}

void task_setTrainTrangleShape(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("triangle");
}

void task_setTrainLeftRight(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("left-right");
}

void task_setTrainUpDown(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("up-down");
}

void task_setTrainLetterW(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("letter-w");
}

void task_setTrainLetterH(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("letter-h");
}

void task_setTrainLetterR(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("letter-r");
}

void task_setTrainIdle(int cmd_idx)
{
    SoundPlayer::getInstance()->speak(VOICE_OK);
    SampleRecorder::getInstance()->setLabel("idle");
}

asr_task_cmd_t asr_training_mode_task_list[] = {
    {
        "xun lian yuan xing",  //训练圆形
        NULL,
        task_setTrainCircleShape
    },
    {
        "xun lian fang xing",  //训练方形
        NULL,
        task_setTrainSquareShape
    },
    {
        "xun lian dui hao",  //训练对号
        NULL,
        task_setTrainCheckShape
    },
    {
        "xun lian jia hao",  //训练加号
        NULL,
        task_setTrainPlusShape
    },
    {
        "xun lian cuo hao",  //训练错号
        NULL,
        task_setTrainCrossShape
    },
    {
        "xun lian da yu hao",  //训练大于号
        NULL,
        task_setTrainGreatMarkShape
    },
    {
        "xun lian xiao yu hao",  //训练小于号
        NULL,
        task_setTrainLessMarkShape
    },
    {
        "xun lian san jiao xing",  //训练三角形
        NULL,
        task_setTrainTrangleShape
    },
    {
        "xun lian zuo you bai dong",  //训练左右摆动
        NULL,
        task_setTrainLeftRight
    },
    {
        "xun lian shang xia bai dong",  //训练上下摆动
        NULL,
        task_setTrainUpDown
    },
    {
        "xun lian zi mu da bu liu",  //训练字母W
        NULL,
        task_setTrainLetterW
    },
    {
        "xun lian zi mu ai chi",  //训练字母H
        NULL,
        task_setTrainLetterH
    },
    {
        "xun lian zi mu a",  //训练字母R
        NULL,
        task_setTrainLetterR
    },
    {
        "xun lian kong xian zhuang tai",  //训练空闲状态
        NULL,
        task_setTrainIdle
    },
    { NULL, NULL, NULL }
};


int ASRTaskMgr::updateTrainingModeCmdList()
{
    esp_err_t rc = ESP_OK;
    for(int i = 0; i< ASR_TRAININGMODE_CMD_NUM(); i++) {
        if(asr_training_mode_task_list[i].cmd_msg==NULL) break;
        int cmdIdx = ASR_TRAININGMODE_CMD_INDEX(i);
        ASRTaskMgr::updateCmd(cmdIdx, asr_training_mode_task_list[i].cmd_msg);
    }
    return rc;
}