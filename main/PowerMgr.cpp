#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "hal/adc_types.h"
#include "smart_utility.h"
#include "PowerMgr.h"
#include "PinDef.h"
#include "SoundPlayer.h"
#include "AudioMgr.h"


extern "C" int adc_read_voltage(adc_oneshot_unit_handle_t adc1_handle, int channel_number);
extern "C" void enableULP(void);
extern "C" void ulpSetStopFlag();
extern "C" void ulpRunAfterWakeUp(void);
const static char *TAG = "PowerMgr";
static PowerMgr g_PowerMgr;

struct capacity {
	int capacity;
	int min;
	int max;	
	int offset;
	int hysteresis;
};

int adc_offset = 0; //adc voltage offset, unit mv
#define BATTERY_LI_CAP_TABLE_ROWS   12
static struct capacity battery_capacity_tables_li[]= {
   /*percentage, min, max, hysteresis*/
	{0, 0, 3426, 0, 10},
	{1, 3427, 3638, 0, 10},
	{10,3639, 3697, 0, 10},
	{20,3698, 3729, 0, 10},
	{30,3730, 3748, 0, 10},
	{40,3749, 3776, 0, 10},
	{50,3777, 3827, 0, 10},
	{60,3828, 3895, 0, 10},
	{70,3896, 3954, 0, 10},
	{80,3955, 4050, 0, 10},
	{90,4051, 4119, 0, 10},
	{100,4120,4240, 0, 10},
};

static int find_item_by_voltage(int voltage)
{
    for(int i=0; i< BATTERY_LI_CAP_TABLE_ROWS; i++)
    {
        if(voltage >= battery_capacity_tables_li[i].min 
            && voltage <= battery_capacity_tables_li[i].max) return battery_capacity_tables_li[i].capacity;
    }

    return -1;
}

void PowerMgr::stopADC1()
{
    if(m_adc1Handle==NULL) return;
    adc_oneshot_del_unit(m_adc1Handle);
}

int PowerMgr::readBatteryVoltage()
{
    int voltage = 0;
    voltage = adc_read_voltage(m_adc1Handle, ADC_CHAN_BATTERY);
    if(voltage < 0) {
        ESP_LOGE(TAG, "Failed to read battery voltage from ADC: %d.", voltage);
        return ESP_FAIL;
    }

    voltage = (voltage - adc_offset) * 2; //R = 100K + 100K

    ESP_LOGW(TAG, "Battery voltage %dmv.", voltage);
    return voltage;
}

int PowerMgr::getPowerPercentage()
{
    enableBatteryVoltageADC();
    vTaskDelay(pdMS_TO_TICKS(1));  //wait 1ms to ensure everythig is ready for ADC 

    int vt = readBatteryVoltage();
    
    disableBatteryVoltageADC();

    if(vt<0) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Battery voltage: %dmv.", vt);

    int cp = find_item_by_voltage(vt);
    if(cp < 0) {
        ESP_LOGE(TAG, "Failed to map battery voltage(%d) to capacity.", vt);
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Battery capacity %d%%.", cp);
    return cp;
}

void PowerMgr::vTimerCallback(void *ctx)
{
    PowerMgr *handle = ( PowerMgr* ) ctx;
    uint8_t nLowPowerCount = 0;
    while(1) {
        handle->m_nPowerPercentage = handle->getPowerPercentage();

        if(handle->m_nPowerPercentage<=20 && handle->m_nTimerRunCount%5==0) {       //warning every 5min
            SoundPlayer *sndPlayer = SoundPlayer::getInstance();
            sndPlayer->speakWithNum("EB-PWRNO", handle->m_nPowerPercentage);
        }
        if(handle->m_nPowerPercentage<=0) {        //power is too low. sleep immediately
            if(nLowPowerCount>=3) {
                handle->tryDeepSleep(true);
            }else{
                nLowPowerCount++;
            }
        }else{
            nLowPowerCount = 0;
        }
        handle->m_nTimerRunCount++;

        handle->tryDeepSleep(false);
        vTaskDelay(pdMS_TO_TICKS(60*1000));
    }
}

int PowerMgr::init()
{
    updateInputActivityTime();
    runAfterWakeUp();
    initADC1();

    gpio_set_direction(GPIO_VCC5V_EN, GPIO_MODE_OUTPUT);
    powerOff5V();
    gpio_set_direction(GPIO_PA_EN, GPIO_MODE_OUTPUT);  
    disablePA();
    gpio_set_direction(GPIO_BAT_ADC_EN, GPIO_MODE_OUTPUT);
    disableBatteryVoltageADC();
    gpio_set_direction(GPIO_LEDSTRIP_EN, GPIO_MODE_OUTPUT);
    disableLEDStrip();
    gpio_set_direction(GPIO_LEDLIGHT_SW, GPIO_MODE_OUTPUT);
    turnOffLEDLight();
    gpio_set_direction(GPIO_IRMRX_PWR, GPIO_MODE_OUTPUT);
    turnOffIRMPWR();
    gpio_set_direction(GPIO_LDR_PWR, GPIO_MODE_OUTPUT);
    turnOffLDR();

    turnOffLCDPWR();
    turnOffLCDBL();

    xTaskCreate(PowerMgr::vTimerCallback, "power_monitor_timer", 1024+1024+512, (void*)this, 1, NULL);
    //m_hTimer = xTimerCreate("power_monitor_timer",  pdMS_TO_TICKS( 60*1000 ), pdTRUE, this, PowerMgr::vTimerCallback);
    //if(m_hTimer==NULL) return ESP_FAIL;

    //xTimerStart(m_hTimer, pdMS_TO_TICKS( 10*1000 ));
    
    return ESP_OK;
}

void PowerMgr::initADC1()
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &m_adc1Handle));
}

void PowerMgr::runAfterWakeUp()
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if(cause!=ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGW(TAG, "A wakeup is detected, reason = %d.", cause);
        rtc_gpio_deinit(GPIO_POWERON);
        rtc_gpio_deinit(GPIO_RADAR_INT);
        ulpSetStopFlag();
    }
    if(cause==ESP_SLEEP_WAKEUP_ULP) {
        ulpRunAfterWakeUp();
    }
}

int PowerMgr::stop()
{
    //BaseType_t rc = xTimerStop(m_hTimer, pdMS_TO_TICKS(1000));
    //if(rc==pdFAIL) return ESP_FAIL;
    return ESP_OK;
}

PowerMgr* PowerMgr::getInstance()
{
    return &g_PowerMgr;
}


void PowerMgr::powerOff5V()
{
    gpio_set_level(GPIO_VCC5V_EN, 0);
}


void PowerMgr::disablePA()
{
    gpio_set_level(GPIO_PA_EN, 0);
}

void PowerMgr::powerOn5V()
{
    gpio_set_level(GPIO_VCC5V_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(20));      //wait for 20ms
}


void PowerMgr::enablePA()
{
    gpio_set_level(GPIO_PA_EN, 1);      //enable PA
}

void PowerMgr::enableBatteryVoltageADC()
{
    gpio_set_level(GPIO_BAT_ADC_EN, 0);
}

void PowerMgr::disableBatteryVoltageADC()
{
    gpio_set_level(GPIO_BAT_ADC_EN, 1);
}

void PowerMgr::enableLEDStrip()
{
    powerOn5V();
    gpio_set_level(GPIO_LEDSTRIP_EN, 0);
}

void PowerMgr::disableLEDStrip()
{
    gpio_set_level(GPIO_LEDSTRIP_EN, 1);
}

void PowerMgr::turnOnLEDLight()
{
    gpio_set_level(GPIO_LEDLIGHT_SW, 1);
}

void PowerMgr::turnOffLEDLight()
{
    gpio_set_level(GPIO_LEDLIGHT_SW, 0);
}

void PowerMgr::turnOnLDR()
{
    gpio_set_direction(GPIO_LDR_PWR, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LDR_PWR, 1);
}

void PowerMgr::turnOffLDR()
{
    gpio_set_level(GPIO_LDR_PWR, 0);
}

void PowerMgr::turnOnIRMPWR()
{
    gpio_set_direction(GPIO_IRMRX_PWR, GPIO_MODE_OUTPUT); //the state of the GPIO may be changed by RMT
    gpio_set_level(GPIO_IRMRX_PWR, 0);
}

void PowerMgr::turnOffIRMPWR()
{
    gpio_set_direction(GPIO_IRMRX_PWR, GPIO_MODE_OUTPUT);  //the state of the GPIO may be changed by RMT
    gpio_set_level(GPIO_IRMRX_PWR, 1);
}

void PowerMgr::turnOnLCDPWR()
{
    gpio_set_direction(GPIO_LCD_PWR_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LCD_PWR_EN, 1);
}

void PowerMgr::turnOffLCDPWR()
{
    gpio_set_direction(GPIO_LCD_PWR_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LCD_PWR_EN, 0);
}

void PowerMgr::turnOnLCDBL()
{
    gpio_set_direction(GPIO_LCD_BL_SW, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LCD_BL_SW, 1);
}

void PowerMgr::turnOffLCDBL()
{
    gpio_set_direction(GPIO_LCD_BL_SW, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LCD_BL_SW, 0);
}


void PowerMgr::tryDeepSleep(bool bForceSleep)
{

    int64_t n = esp_timer_get_time() - m_nLastInputActivityTime;
    if(bForceSleep==false && n < 5 * 60 * 1000 * 1000) return;

    ESP_LOGW(TAG, "Entering deep sleep logic...");
    SoundPlayer *sndPlayer = SoundPlayer::getInstance();
    if(bForceSleep==true) sndPlayer->speak("EB-PWRDN", true);
    else sndPlayer->speak("EB-GOSLP", true);

    enterSleep();
}

void PowerMgr::enterSleep(bool bEnableULP)
{
    //turn off power for sensors
    AudioMgr::getInstance()->deinit();
    powerOff5V();                                              
    turnOffIRMPWR();
    turnOffLEDLight();
    turnOffLDR();
    disableLEDStrip();
    disableBatteryVoltageADC();

    if(m_bEnableNightLight==true) bEnableULP = true;

    runBeforeDeepSleep(bEnableULP);
    //These are not RTC GPIO. Need to figure out how to avoid current leakge during deep sleep
    //rtc_gpio_isolate(GPIO_IDF_I2C_SCL);     //because it is pull up externally
    //rtc_gpio_isolate(GPIO_IDF_I2C_SDA);     //because it is pull up externally
    //rtc_gpio_isolate(GPIO_ADF_I2C_SCL);     //because it is pull up externally
    //rtc_gpio_isolate(GPIO_ADF_I2C_SDA);     //because it is pull up externally

    stopADC1();
    if(bEnableULP) {
        if(m_bEnableNightLight) {
            powerOn5V();  
            //gpio_hold_en(GPIO_VCC5V_EN);
        }
        enableULP();
    }

    gpio_deep_sleep_hold_en();
    esp_deep_sleep_start();
}

void PowerMgr::runBeforeDeepSleep(bool bEnableULP)
{
    ESP_LOGI(TAG, "m_bEnableNightLight = %d.", m_bEnableNightLight);
    ESP_LOGI(TAG, "bEnableULP = %d.", bEnableULP);

    //configure deep sleep wake up source
    // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
    // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
    // No need to keep that power domain explicitly, unlike EXT1.
    ESP_LOGI(TAG, "Enabling EXT0 wakeup on pin GPIO%d\n", GPIO_POWERON);
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(GPIO_POWERON, 0));
    rtc_gpio_pullup_en(GPIO_POWERON);           //trggier by low level, use internal pullup(no external pullup)
    rtc_gpio_pulldown_dis(GPIO_POWERON);

    //for radar PIN. It can either wake up the main CPU, or wake up ULP
    if(bEnableULP==false && m_bEnableNightLight==false) {
        ESP_LOGI(TAG, "Enabling EXT0 wakeup on pin GPIO%d\n", GPIO_RADAR_INT);
        ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(GPIO_RADAR_INT, 1));
        rtc_gpio_pullup_dis(GPIO_POWERON);       //trggier by high level, use internal pulldown(no external pulldown)
        rtc_gpio_pulldown_en(GPIO_POWERON);
    }
}

void PowerMgr::updateInputActivityTime()
{
    m_nLastInputActivityTime = esp_timer_get_time();
}