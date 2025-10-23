#include "esp_log.h"
#include "lv_conf.h"
#include "lvgl.h"
#include "LCDDev.h"
#include "MBaseUI.h"

static const char *TAG = "MNetConnectUI";
LV_FONT_DECLARE(lv_font_siyuan_16);

int MNetConnectUI::initUI()
{
    //lv_arc_set_angles
    static lv_style_t style;                     //创建样式
    static lv_style_t bc_style;
 
    lv_style_init(&style);                       //初始化样式
    lv_style_set_arc_color(&style, lv_palette_main(LV_PALETTE_RED)); //设置圆弧颜色
    lv_style_set_arc_width(&style, 20);            //设置圆弧宽度；
 
    lv_style_init(&bc_style);                       //初始化样式
    lv_style_set_arc_color(&bc_style, lv_palette_main(LV_PALETTE_YELLOW)); //设置背景圆环颜色
    lv_style_set_arc_width(&bc_style, 20);        //设置背景圆环宽度

    /*Create a spinner*/
    lv_obj_t * spinner = lv_spinner_create(lv_scr_act());
    lv_spinner_set_anim_params(spinner, 1000, 45);
    lv_obj_add_style(spinner, &style, LV_PART_INDICATOR);//应用到圆弧部分；
    lv_obj_add_style(spinner, &bc_style, LV_PART_MAIN);//应用到背景圆环部分；

    lv_obj_set_size(spinner, 100, 100);
    lv_obj_center(spinner);
    //lv_spinner_set_anim_params(spinner, 10000, 200);

    /*Create label under the spinner*/
    lv_obj_t * cz_label = lv_label_create(lv_scr_act());
    lv_label_set_long_mode(cz_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(cz_label, &lv_font_siyuan_16, 0);
    lv_obj_set_width(cz_label, 200);
    lv_obj_align(cz_label, LV_ALIGN_BOTTOM_LEFT, 50, -30);

    m_txtLabelHandle = cz_label;

    return 0;
}

int MNetConnectUI::init()
{
    ESP_LOGI(TAG, "Initializing MNetConnectUI...");
    int rc = 0;
    if(LCDDev::getInstance()->lock(-1)) {
        rc = initUI();
        LCDDev::getInstance()->unlock();
    }

    ESP_LOGI(TAG, "Finished MNetConnectUI initialization(rc=%d).", rc);
    return rc;
}


void MNetConnectUI::setText(const char *txt)
{
    lv_obj_t * cz_label = (lv_obj_t *)m_txtLabelHandle;

    if(LCDDev::getInstance()->lock(-1)) {
        lv_label_set_text(cz_label, txt);
        LCDDev::getInstance()->unlock();
    }
}
