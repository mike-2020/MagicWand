#include "esp_log.h"
#include "lv_conf.h"
#include "lvgl.h"
#include "LCDDev.h"
#include "MBaseUI.h"

LV_FONT_DECLARE(lv_font_siyuan_32);

int MSplashUI::init()
{
    int rc = 0;
    if(LCDDev::getInstance()->lock(-1)) {
        rc = initUI();
        LCDDev::getInstance()->unlock();
    }
    return rc;
}

int MSplashUI::initUI()
{
    lv_obj_t * cz_label = lv_label_create(lv_scr_act());
    lv_label_set_long_mode(cz_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(cz_label, &lv_font_siyuan_32, 0);
    lv_obj_set_width(cz_label, 200);
    lv_obj_align(cz_label, LV_ALIGN_CENTER, 40, 0);
    lv_label_set_text(cz_label, "神奇魔杖");

    m_txtLabelHandle = cz_label;
    return 0;
}