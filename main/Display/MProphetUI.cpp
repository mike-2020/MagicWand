#include <string.h>
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "lvgl.h"
#include "MBaseUI.h"
#include "LCDDev.h"

LV_FONT_DECLARE(lv_font_harmony_16);
static const char *TAG = "MProphetUI";

int MProphetUI::init()
{
    RingbufHandle_t buf_handle;
    buf_handle = xRingbufferCreateNoSplit(15*2+1, 30);
    //xRingbufferReceiveSplit();
    if (buf_handle == NULL) {
        printf("Failed to create ring buffer\n");
    }
    m_buffHandle = (void*)buf_handle;

    if(LCDDev::getInstance()->lock(-1)) {
        initUI();
        LCDDev::getInstance()->unlock();
    }
    return 0;

}

#define TEXT_ROWS   5

int MProphetUI::initUI()
{
    lv_obj_t*  bg = lv_obj_create(NULL);//创建背景界面
    lv_obj_set_style_bg_color(bg, lv_color_make(255,255,255), 0);//设置背景颜色为白色
    lv_obj_set_scrollbar_mode(bg, LV_SCROLLBAR_MODE_OFF);//关闭界面的滚动条
    lv_scr_load(bg);

    //lv_obj_t *img = lv_img_create(lv_scr_act());
    /* Assuming a File system is attached to letter 'A'
     * E.g. set LV_USE_FS_STDIO 'A' in lv_conf.h */
    //lv_img_set_src(img, "A:/theme1/img/small.png");
    //lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *img = lv_gif_create(lv_scr_act());
    lv_gif_set_src(img, "A:/theme1/img/cai.gif");
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 20);


    //lv_obj_t * lottie = lv_lottie_create(lv_screen_active());
    /*If there are no special requirements, just declare a buffer
      x4 because the Lottie is rendered in ARGB8888 format*/
    //uint8_t *buf = (uint8_t*)malloc(150 * 150 * 4);
    //lv_lottie_set_buffer(lottie, 150, 150, buf);
    //lv_lottie_set_src_file(lottie, "/res/theme1/img/prophet.ltt");
    //lv_obj_center(lottie);


    lv_obj_t * txt_label = lv_label_create(lv_scr_act());
    lv_label_set_text(txt_label, "");
    lv_label_set_long_mode(txt_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(txt_label, &lv_font_harmony_16, 0);
    lv_obj_set_size(txt_label, 200, TEXT_ROWS*16);
    lv_obj_align(txt_label, LV_ALIGN_BOTTOM_LEFT, 10, -50);
    m_txtLabelHandle = (void*)txt_label;

    lv_timer_create((lv_timer_cb_t)MProphetUI::cbUpdateText, 1000, this);//创建一个定时器，每秒更新一次

    return 0;
}

void MProphetUI::appendText(const char *msg)
{
    char buff[15*2+1];
    const char *pHead = msg;
    const char *pTail = msg + strlen(msg);
    while(pHead <= pTail) {
        if(*pHead=='\0') {
            pHead++;
            continue;
        }
        memset(buff, 0, sizeof(buff));
        strncpy(buff, pHead, 15*2);
        pHead += strlen(buff);
        UBaseType_t res =  xRingbufferSend((RingbufHandle_t)m_buffHandle, buff, strlen(buff)+1, pdMS_TO_TICKS(100));
        if (res != pdTRUE) {
            ESP_LOGI(TAG, "Failed to send item\n");
        }
    }
}

void MProphetUI::cbUpdateText(void *timer)
{
    MProphetUI *handle = (MProphetUI*)lv_timer_get_user_data((lv_timer_t *)timer);
    RingbufHandle_t buf_handle = (RingbufHandle_t)handle->m_buffHandle;
    lv_obj_t *txtLabel = (lv_obj_t*)handle->m_txtLabelHandle;

    //从字节 buffer 中接收数据
    size_t item_size;
    char *item = (char *)xRingbufferReceive(buf_handle, &item_size, pdMS_TO_TICKS(100));
    if (item == NULL) {
        //接收数据项失败
        //printf("Failed to receive item\n");
        return;
    }
    ESP_LOGI(TAG, "Received item: %s", item);
    if(LCDDev::getInstance()->lock(-1)) {
        lv_label_cut_text(txtLabel, 0, strlen(item));  //remove the first line
        lv_label_ins_text(txtLabel, LV_LABEL_POS_LAST, item); //append the last line
        LCDDev::getInstance()->unlock();
    }

    vRingbufferReturnItem(buf_handle, (void *)item);
}

