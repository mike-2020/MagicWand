#include "lvgl.h"
#include "LCDDev.h"
#include "MBaseUI.h"

int MBaseUI::deinit()
{
    if(LCDDev::getInstance()->lock(-1)) {
        lv_obj_remove_style_all(lv_scr_act());
        lv_obj_clean(lv_scr_act());
        LCDDev::getInstance()->unlock();
    }
    return 0;
}

