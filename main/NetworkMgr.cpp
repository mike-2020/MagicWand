#include "smart_utility.h"
#include "esp_wifi.h"
#include "SoundPlayer.h"
#include "MBaseUI.h"
#include "NetworkMgr.h"

static NetworkMgr g_networkMgr;

NetworkMgr* NetworkMgr::getInstance()
{
    return &g_networkMgr;
}

int NetworkMgr::init(bool bShowUI)
{
    int rc = 0;
    MNetConnectUI *netUI = NULL;
    if(bShowUI==true) {
        netUI = (MNetConnectUI*)MUIMgr::getInstance()->setUI(MUI_NETCONNECT);;
        netUI->setText("正在建立WIFI连接......");
    }

    //SoundPlayer::getInstance()->speak(VOICE_CONNECT_WIFI);
    wifi_init(false, false);
    int rssi = 0;
    rc = esp_wifi_sta_get_rssi(&rssi);
    if(rc!=ESP_OK) {
        SoundPlayer::getInstance()->speak(VOICE_WIFI_FAIL);
    }else{
        //SoundPlayer::getInstance()->speak(VOICE_WIFI_SUCCESS);
        if(bShowUI==true)netUI->setText("正在同步系统时间......");
        start_sntp_service();
        //start_file_server("/mp3");
    }

    return 0;
}

int NetworkMgr::deinit()
{
    return 0;
}


int NetworkMgr::getWIFIRSSI()
{
    int rc = 0;
    int rssi = 0;
    return -1;
    rc = esp_wifi_sta_get_rssi(&rssi);
    if(rc!=ESP_OK) return ESP_FAIL;
    return rssi;
}