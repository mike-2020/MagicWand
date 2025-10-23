#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "iot_button.h"
#include "HMIMgr.h"
#include "SampleRecorder.h"
#include "GestureRecognizer.h"
#include "SoundPlayer.h"
#include "ASRTaskMgr.h"
#include "MagicWand.h"
#include "RemoteController.h"
#include "PinDef.h"
#include "PowerMgr.h"


#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_RECORDER_BTN)
static HMIMgr g_hmiMgr;
static const char *TAG = "HMIMgr";

static button_handle_t g_btn_center, g_btn_left, g_btn_right, g_btn_up, g_btn_down;
static hmi_btn_t g_btn_list[5];

static int64_t last_trigger_time = 0;
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    HMIMgr *handle = (HMIMgr*) arg;
    uint32_t cmd = 0;
    int64_t tm = esp_timer_get_time();
    if(tm - last_trigger_time < 500*1000) {     //ignore new triggers in 0.2s
        return;
    }
    last_trigger_time = tm;

    //int pin_level = gpio_get_level(GPIO_RECORDER_BTN);
    //if(pin_level==0) {                      //button is pressed, trigger data capture
        cmd = HMI_CMD_BTN_PRESS;
        xQueueSendFromISR(handle->m_cmdQueue, &cmd, NULL);
    //} //else{                                  //button is released, stop data capture
    //    handle->m_stopRecordFlag = true;
        //cmd = 2;
    //}

    
}

HMIMgr* HMIMgr::getInstance()
{
    return &g_hmiMgr;
}

HMIMgr::HMIMgr()
{
    m_nWorkingMode=WORKING_MODE_WAND;
    m_curWMInstance = NULL;
}

int HMIMgr::getBtnState()
{
    return gpio_get_level(GPIO_RECORDER_BTN);
}

void HMIMgr::turnOnLED()
{
    PowerMgr::getInstance()->turnOnLEDLight();
}

void HMIMgr::turnOffLED()
{
    PowerMgr::getInstance()->turnOffLEDLight();
}


int HMIMgr::init()
{
#if 0
    gpio_config_t io_conf = {};

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)1;
    gpio_config(&io_conf);

    //change gpio interrupt type for one pin
    gpio_set_intr_type(GPIO_RECORDER_BTN, GPIO_INTR_NEGEDGE);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_RECORDER_BTN, gpio_isr_handler, (void*) this);

#endif

    // create gpio button
    button_config_t gpio_btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = GPIO_BTN_CENTER,
            .active_level = 0,
        },
    };
    g_btn_center = iot_button_create(&gpio_btn_cfg);
    if(NULL == g_btn_center) {
        ESP_LOGE(TAG, "Button create failed");
    }
    iot_button_register_cb(g_btn_center, BUTTON_PRESS_DOWN, HMIMgr::onBtnCenterPressDown, &g_btn_list[BTN_NUM_CENTER]);
    iot_button_register_cb(g_btn_center, BUTTON_PRESS_UP, HMIMgr::onBtnCenterPressUp, &g_btn_list[BTN_NUM_CENTER]);

    // create adc button
    //On ESP32S3, ADC_ATTEN_DB_12 is used by ADC, measurable input voltage range 150mv - 2450 mV
    button_config_t adc_btn_cfg = {
        .type = BUTTON_TYPE_ADC,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .adc_button_config = {
            .adc_channel = ADC_CHN_BTN,
            .button_index = 0,
            .min = 1500,
            .max = 1700,
        },
    };
    adc_btn_cfg.adc_button_config.adc_handle = PowerMgr::getInstance()->getADC1HandlePtr();
    g_btn_right = iot_button_create(&adc_btn_cfg);
    if(NULL == g_btn_right) {
        ESP_LOGE(TAG, "Button create failed");
    }
    iot_button_register_cb(g_btn_right, BUTTON_SINGLE_CLICK, HMIMgr::onBtnRightClick, &g_btn_list[BTN_NUM_RIGHT]);

    adc_btn_cfg.adc_button_config.button_index = 1;
    adc_btn_cfg.adc_button_config.min = 1000;
    adc_btn_cfg.adc_button_config.max = 1300;
    g_btn_left = iot_button_create(&adc_btn_cfg);
    if(NULL == g_btn_left) {
        ESP_LOGE(TAG, "Button create failed");
    }
    iot_button_register_cb(g_btn_left, BUTTON_SINGLE_CLICK, HMIMgr::onBtnLeftClick, &g_btn_list[BTN_NUM_LEFT]);

    //create a queue to handle gpio event from isr
    m_cmdQueue = xQueueCreate(1, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(HMIMgr::proc, "HMIMgr", 1024*3, (void*)this, 10, NULL);

    setWorkingMode(WORKING_MODE_WAND);
    //MagicWand::getInstance()->start();
    ESP_LOGI(TAG, "HMIMgr has been initialized.");
    return 0;
}


void HMIMgr::registerBtnEvent(int btnNum, button_event_t evtType, hmi_btn_cb_t cb, void *ctx)
{
    switch(evtType) {
        case BUTTON_PRESS_DOWN:
            g_btn_list[btnNum].cb_pressdown = cb;
            break;
        case BUTTON_PRESS_UP:
            g_btn_list[btnNum].cb_pressup = cb;
            break;
    }
    g_btn_list[btnNum].ctx = ctx;
}

void HMIMgr::onBtnCenterPressDown(void *btnHandle, void *ctx)
{
    hmi_btn_t *btn = (hmi_btn_t*)ctx;
    if(btn->cb_pressdown!=NULL) {
        btn->cb_pressdown(btn->ctx);
    } 
    PowerMgr::getInstance()->updateInputActivityTime();
}

void HMIMgr::onBtnCenterPressUp(void *btnHandle, void *ctx)
{
    hmi_btn_t *btn = (hmi_btn_t*)ctx;
    if(btn->cb_pressup!=NULL) {
        btn->cb_pressup(btn->ctx);
    } 
    PowerMgr::getInstance()->updateInputActivityTime();
}

void HMIMgr::onBtnLeftClick(void *btnHandle, void *ctx)
{
    ESP_LOGI(TAG, "Left button was clicked.");
}

void HMIMgr::onBtnRightClick(void *btnHandle, void *ctx)
{
    ESP_LOGI(TAG, "Right button was clicked.");
}

void HMIMgr::proc(void *ctx)
{
    HMIMgr *handle = (HMIMgr*)ctx;
    uint32_t cmd = 1;
    while(1) {
        handle->blinkLED();
        if (xQueueReceive(handle->m_cmdQueue, &cmd, pdMS_TO_TICKS(200))!=pdTRUE) continue;
        ESP_LOGW(TAG, "Received command %d.", cmd);
        switch(cmd) {
            case HMI_CMD_BTN_PRESS:
                handle->onBtnPressed();
                break;
            case HMI_CMD_MODE_NORMAL:
                handle->setWorkingMode(WORKING_MODE_WAND);
                break;
            case HMI_CMD_MODE_TRAINING:
                handle->setWorkingMode(WORKING_MODE_TRAINING);
                break;
            case HMI_CMD_MODE_CONTROLLER:
                handle->setWorkingMode(WORKING_MODE_CONTROLLER);
                break;
        }
        
    }
}


void HMIMgr::setWorkingMode(int wMode)
{
    if(m_curWMInstance!=NULL) {
        m_curWMInstance->stop();
        m_curWMInstance = NULL;
    }
    switch(wMode) {
        case WORKING_MODE_WAND:
            m_curWMInstance = MagicWand::getInstance();
            break;
        case WORKING_MODE_TRAINING:
            m_curWMInstance = SampleRecorder::getInstance();
            break;
        case WORKING_MODE_CONTROLLER:
            m_curWMInstance = RemoteController::getInstance();
            break;
    }
    if(m_curWMInstance!=NULL) {
        m_curWMInstance->start();
    }
    ESP_LOGI(TAG, "Setting working mode to 0x%x.", wMode);
    m_nWorkingMode = wMode;
}

void HMIMgr::onBtnPressed()
{
    PowerMgr::getInstance()->updateInputActivityTime();
    
    if(m_nWorkingMode==WORKING_MODE_TRAINING) {
        //
        if(SampleRecorder::getInstance()->getIdleState()==true) {
            training_cmd_t cmd;
            cmd.cmd = SR_CMD_TRIGGER;
            SampleRecorder::getInstance()->sendCmd(&cmd);
        }

    }else if(m_nWorkingMode==WORKING_MODE_WAND) {
        if(GestureRecognizer::getInstance()->getIdleState()==true) {
            GestureRecognizer::getInstance()->sendCmd(GR_CMD_TRIGGER);
        }
    }
}

int HMIMgr::sendCmd(uint32_t cmd)
{
    if(m_cmdQueue==NULL) {
        ESP_LOGE(TAG, "m_cmdQueue is NULL.");
        return ESP_FAIL;
    }
    BaseType_t rc = xQueueSend(m_cmdQueue, &cmd, pdMS_TO_TICKS(100));
    return rc;
}

void HMIMgr::blinkLED()
{
    if(m_bLEDBlink==false) return;
    static bool ledON = false;
    if(ledON) {
        turnOnLED();
        ledON = false;
    }else{
        turnOffLED();
        ledON = true;
    }
}