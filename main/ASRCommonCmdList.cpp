#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "smart_utility.h"
#include "ASRTaskMgr.h"
#include "SampleRecorder.h"
#include "SoundPlayer.h"
#include "HMIMgr.h"
#include "PowerMgr.h"
#include "IRMTransceiver.h"
#include "LedStrip.h"

#define ASR_COMMON_CMD_INDEX(X)                 (X)
#define ASR_COMMON_CMD_NUM()                    (sizeof(asr_common_task_list)/sizeof(asr_task_cmd_t))
static const char *TAG = "ASRTaskMgr";

void task_exit_wakeup()
{

}

void task_switch2NormalMode(int cmd_idx)
{
    HMIMgr::getInstance()->sendCmd(HMI_CMD_MODE_NORMAL);
}

void task_switch2TrainingMode(int cmd_idx)
{
    HMIMgr::getInstance()->sendCmd(HMI_CMD_MODE_TRAINING);
}

void task_switch2ControllerMode(int cmd_idx)
{
    HMIMgr::getInstance()->sendCmd(HMI_CMD_MODE_CONTROLLER);
}

void task_volume_increase(int cmd_idx)
{

}

void task_volume_decrease(int cmd_idx)
{

}

void task_volume_maximize(int cmd_idx)
{
    //audio_manager_set_volume(AUDIO_VOLUME_MAX);
}

void task_volume_minimize(int cmd_idx)
{
    //audio_manager_set_volume(AUDIO_VOLUME_MIN);
}

void task_volume_middle(int cmd_idx)
{
    //audio_manager_set_volume(AUDIO_VOLUME_MID);
}

void task_led_light_on(int)
{
    PowerMgr::getInstance()->turnOnLEDLight();
}

void task_led_light_off(int)
{
    PowerMgr::getInstance()->turnOffLEDLight();
}

void task_wifi_password_reset(int)
{
    reset_wifi_cred();
}

void task_wifi_report_info(int)
{
    int rssi = 0;
    esp_err_t rc = ESP_OK;
    char msg[64]="无法取得无线网络信息。";
    rc = esp_wifi_sta_get_rssi(&rssi);
    if(rc==ESP_OK) {
        sprintf(msg, "无线网络信号强度，负%d。", abs(rssi));
        ESP_LOGI(TAG, "%s", msg);
    }
}

void task_ir_learn(int)
{
    IRMTransceiver::startIRLearn();
}

void task_night_light_test(int)
{
    PowerMgr::getInstance()->enableNightLight();
    PowerMgr::getInstance()->enterSleep(true);
}

void task_night_light_enable(int)
{
    PowerMgr::getInstance()->enableNightLight();
}

void task_night_light_disable(int)
{
    PowerMgr::getInstance()->disableNightLight();
}

void task_led_strip_blink(int)
{
    LEDStrip::getInstance()->sendCmd(LED_CMD_BLINK_FLOW);
}

asr_task_cmd_t asr_common_task_list[] = {
    { "", "", NULL},
    {
        "tui xia",  //退下
        "主人，小志先退下了，您可以随时用“你好小志”唤醒我。",
        NULL
    },
    {
        "mo zhang mo shi",  //前进
        VOICE_OK,
        task_switch2NormalMode
    },
    {
        "xun lian mo shi",  //训练模式
        NULL,
        task_switch2TrainingMode
    },
    {
        "yao kong qi mo shi",  //遥控器模式
        VOICE_OK,
        task_switch2ControllerMode
    },
    {
        "zui da yin liang,yin liang zui da",  //最大音量
        VOICE_OK,
        task_volume_maximize
    },
    {
        "zui xiao yin liang,yin liang zui xiao",  //最小音量
        VOICE_OK,
        task_volume_minimize
    },
    {
        "zhong deng yin liang,yin liang zhong deng",  //中等音量
        VOICE_OK,
        task_volume_middle
    },
    {
        "da kai zhao ming deng",  //打开照明灯
        VOICE_OK,
        task_led_light_on
    },
    {
        "guan bi zhao ming deng",  //关闭照明灯
        VOICE_OK,
        task_led_light_off
    },
    {
        "chong zhi wai fei mi ma",  //重置WIFI密码
        "密码已经重置，需要重新启动才能生效。",
        task_wifi_password_reset
    },
    {
        "bao gao wai fei xin xi",  //报告WIFI信息
        NULL,
        task_wifi_report_info
    },
    {
        "xue xi hong wai bian ma,hong wai bian ma xue xi",  //学习红外编码
        VOICE_OK,
        task_ir_learn
    },
    {
        "shi neng xiao ye deng, kai qi xiao ye deng",  //开启小夜灯
        VOICE_OK,
        task_night_light_enable
    },
    {
        "guan bi xiao ye deng",  //关闭小夜灯
        VOICE_OK,
        task_night_light_disable
    },
    {
        "ce shi xiao ye deng",  //测试小夜灯
        VOICE_OK,
        task_night_light_test
    },
    {
        "ce shi deng dai",      //测试灯带
        VOICE_OK,
        task_led_strip_blink
    },
    { NULL, NULL, NULL }
};


int ASRTaskMgr::updateCommonCmdList()
{
    esp_err_t rc = ESP_OK;
    for(int i = 0; i< ASR_COMMON_CMD_NUM(); i++) {
        if(asr_common_task_list[i].cmd_msg==NULL) break;
        if(strlen(asr_common_task_list[i].cmd_msg)==0) continue;
        int cmdIdx = ASR_COMMON_CMD_INDEX(i);
        ASRTaskMgr::updateCmd(cmdIdx, asr_common_task_list[i].cmd_msg);
    }
    ESP_LOGI(TAG, "Common voice command list has been updated.");
    return rc;
}
