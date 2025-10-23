#include "STTService.h"
#include "MM_LLMService.h"
#include "MM_TTSService.h"
#include "LCDDev.h"
#include "MBaseUI.h"
#include "Prophet.h"

static Prophet g_prophetHandle;

Prophet* Prophet::getInstance()
{
    return &g_prophetHandle;
}

int Prophet::start()
{
    //open LCD
    //LCDDev::getInstance()->init();
    //check WIFI

    //check time

    //start service
    MM_BaseService::getLLMInstance()->start();
    MM_BaseService::getTTSInstance()->start();
    STTService::getInstance()->start();
    //show portrait
    MUIMgr::getInstance()->setUI(MUI_PROPHET);
    return 0;
}

int Prophet::stop()
{
    //stop service
    STTService::getInstance()->stop();
    MM_BaseService::getLLMInstance()->stop();
    MM_BaseService::getTTSInstance()->stop();
    //stop WIFI
}

