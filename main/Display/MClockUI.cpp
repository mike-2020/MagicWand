#include <time.h>
#include <stdio.h>
#include "esp_log.h"
#include "lv_conf.h"
#include "lvgl.h"
#include "PowerMgr.h"
#include "NetworkMgr.h"
#include "LCDDev.h"
#include "MBaseUI.h"

static const char *TAG = "MClockUI";

LV_FONT_DECLARE(lv_font_siyuan_32);
LV_FONT_DECLARE(lv_font_siyuan_16);

void MClockUI::cbUpdateTime(void *timer) 
{
    MClockUI *handle = (MClockUI*)lv_timer_get_user_data((lv_timer_t *)timer);

    time_t now;
    struct tm tm;
    const char *weekDays[] = {"日", "一", "二", "三", "四", "五", "六"};
    // 获取当前本地时间
    now = time(NULL);
    localtime_r(&now, &tm);

    char time_str[9]; // 存放格式化后的时间字符串，HH:MM:SS
    char date_str[20]; // 存放格式化后的日期字符串，YYYY-MM-DD 

    // 将时间格式化为字符串，精确到秒
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    // 将日期格式化为字符串，精确到日
    snprintf(date_str, sizeof(date_str), "%04d年%d月%d日", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

    //ESP_LOGI(TAG, "%s %s", date_str, time_str);
    //handle->setDateTimeText(date_str, time_str);

    const char *pwrIcon = NULL;
    int pct = PowerMgr::getInstance()->getPower();
    if(pct>=90) pwrIcon = LV_SYMBOL_BATTERY_FULL;
    else if(pct>70) pwrIcon = LV_SYMBOL_BATTERY_3;
    else if(pct>40) pwrIcon = LV_SYMBOL_BATTERY_2;
    else if(pct>10) pwrIcon = LV_SYMBOL_BATTERY_1;
    else if(pct <= 10) pwrIcon = LV_SYMBOL_BATTERY_EMPTY;

    int rssi = NetworkMgr::getInstance()->getWIFIRSSI();

    if(LCDDev::getInstance()->lock(-1)) {
        lv_label_set_text((lv_obj_t*)handle->m_dateLabelHandle, date_str);
        lv_label_set_text((lv_obj_t*)handle->m_timeLabelHandle, time_str);
        lv_label_set_text((lv_obj_t*)handle->m_batteryLabelHandle, pwrIcon);
        if(rssi>0) lv_obj_set_style_text_color((lv_obj_t*)handle->m_wifiLabelHandle, lv_color_make(255,255,255), 0);
        else lv_obj_set_style_text_color((lv_obj_t*)handle->m_wifiLabelHandle, lv_color_make(50,50,50), 0);
        LCDDev::getInstance()->unlock();
    }
 }



int MClockUI::init()
{
    int rc = 0;
    ESP_LOGI(TAG, "Init CLOCK UI.");

    if(LCDDev::getInstance()->lock(-1)) {
        rc = initUI();
        LCDDev::getInstance()->unlock();
    }
    return rc;
}


int MClockUI::initUI()
{
    lv_obj_t*  time_bg = lv_obj_create(NULL);//创建背景界面
    lv_obj_set_style_bg_color(time_bg,lv_color_make(0,0,0),0);//设置背景颜色为黑色
    lv_obj_set_scrollbar_mode(time_bg, LV_SCROLLBAR_MODE_OFF);//关闭界面的滚动条
    lv_scr_load(time_bg);

    lv_obj_t * label_time = lv_label_create(lv_scr_act());
    lv_label_set_long_mode(label_time, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label_time, 240);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label_time, lv_color_make(255,255,255), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_siyuan_32, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_label_set_text(label_time, "22:10:10");

    m_timeLabelHandle = (void*)label_time;

    lv_obj_t * label_date = lv_label_create(time_bg);//日期控件
    //lv_label_set_long_mode(label_date, LV_LABEL_LONG_EXPAND);
    //lv_obj_set_width(label_date, 200);
    lv_obj_set_size(label_date, 240, 20);
    lv_label_set_text(label_date, "2023/01/01");
    lv_obj_align(label_date,LV_ALIGN_CENTER,0,-25);
    lv_obj_set_style_text_color(label_date, lv_color_make(255,255,255), 0);
    lv_obj_set_style_text_font(label_date, &lv_font_siyuan_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label_date, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    m_dateLabelHandle = (void *)label_date;

    lv_obj_t * label_battery = lv_label_create(time_bg);
    lv_obj_align(label_battery,LV_ALIGN_TOP_MID,15, 10);
    lv_obj_set_style_text_color(label_battery, lv_color_make(255,255,255), 0);
    lv_label_set_text(label_battery, LV_SYMBOL_BATTERY_EMPTY);
    m_batteryLabelHandle = (void*)label_battery;

    lv_obj_t * label_wifi = lv_label_create(time_bg);
    lv_obj_align(label_wifi,LV_ALIGN_TOP_MID,-15,10);
    lv_obj_set_style_text_color(label_wifi, lv_color_make(255,255,255), 0);
    lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
    m_wifiLabelHandle = (void*)label_wifi;

    lv_timer_create((lv_timer_cb_t)MClockUI::cbUpdateTime, 1000, this);//创建一个定时器，每秒更新一次日期、时间、时长
    return 0;
}

void MClockUI::setDateTimeText(const char *dateTxt, const char *timeTxt)
{
    if(LCDDev::getInstance()->lock(-1)) {
        lv_label_set_text((lv_obj_t*)m_dateLabelHandle, dateTxt);
        lv_label_set_text((lv_obj_t*)m_timeLabelHandle, timeTxt);
        LCDDev::getInstance()->unlock();
    }
}