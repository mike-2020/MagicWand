#include <stdio.h>
#include "esp_log.h"
#include "MBaseUI.h"

static const char *TAG = "MUIMGR";
static MUIMgr g_muiMgr;

MUIMgr* MUIMgr::getInstance()
{
    return &g_muiMgr;
}

MBaseUI* MUIMgr::setUI(mui_type_t mui)
{
    if(m_curUI!=NULL) {
        m_curUI->deinit();
        delete m_curUI;
        m_curUI = NULL;
    }
    switch(mui) {
        case MUI_CLOCK:
            ESP_LOGI(TAG, "Switching to CLOCK UI.");
            m_curUI = new MClockUI();
            break;
        case MUI_NETCONNECT:
            ESP_LOGI(TAG, "Switching to NETCONNECT UI.");
            m_curUI = new MNetConnectUI();
            break;
        case MUI_SPLASH:
            ESP_LOGI(TAG, "Switching to SPLASH UI.");
            m_curUI = new MSplashUI();
            break;
        case MUI_PROPHET:
            ESP_LOGI(TAG, "Switching to PROPHET UI.");
            m_curUI = new MProphetUI();
            break;
    }
    if(m_curUI!=NULL) {
        m_curUI->init();
    }
    return m_curUI;
}