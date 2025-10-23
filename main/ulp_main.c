#include <stdio.h>
#include <inttypes.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hal/rtc_io_types.h"
#include "ulp_riscv.h"
#include "ulp_adc.h"
#include "ulp_main.h"
#include "PinDef.h"


extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");
static const char *TAG = "ULP_MAIN";
static void init_ulp_program(void);
static void ulp_gpio_init(void);
static void ulp_adc_config(void);
esp_err_t ulp_adc1_init(const ulp_adc_cfg_t *cfg, adc_channel_t chn2);
esp_err_t ulp_adc1_deinit(void);
static void ulp_gpio_reset(void);
static void ulp_adc_deconfig(void);

void ulpSetStopFlag()
{
    ulp_stop_flag = 1;
}

void enableULP(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    /* not a wakeup from ULP, load the firmware */
    if ((cause != ESP_SLEEP_WAKEUP_ULP) && (cause != ESP_SLEEP_WAKEUP_TIMER)) {
        printf("Not a ULP-RISC-V wakeup (cause = %d), initializing it! \n", cause);
        
        ulp_adc_config();
        ulp_gpio_init();
        init_ulp_program();
    }

    /* RTC peripheral power domain needs to be kept on to keep SAR ADC related configs during sleep */
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup());
}

void ulpRunAfterWakeUp() 
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    ulp_gpio_reset();
    ulp_adc_deconfig();
    /* ULP Risc-V read and detected a temperature above the limit */
    if (cause == ESP_SLEEP_WAKEUP_ULP) {
        ESP_LOGW(TAG, "ULP-RISC-V woke up the main CPU\n");
        ESP_LOGW(TAG, "Threshold: high = %"PRIu32"\n", ulp_adc_threshold);
        ESP_LOGW(TAG, "LDR Value = %"PRIu32"\n", ulp_wakeup_result);
    }

    ulp_stop_flag = 1;
}

static void init_ulp_program(void)
{
    esp_err_t err = ulp_riscv_load_binary(ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start));
    ESP_ERROR_CHECK(err);

    /* The first argument is the period index, which is not used by the ULP-RISC-V timer
     * The second argument is the period in microseconds, which gives a wakeup time period of: 20ms
     */
    //ulp_set_wakeup_period(0, 20000);

    /* Start the program */
    //err = ulp_riscv_run();
    //ESP_ERROR_CHECK(err);
    //ulp_riscv_timer_stop();
    /* Start the program */
    ulp_riscv_cfg_t cfg = {
        .wakeup_source = ULP_RISCV_WAKEUP_SOURCE_GPIO,
    };

    err = ulp_riscv_config_and_run(&cfg);
    ESP_ERROR_CHECK(err);
}

static void ulp_gpio_init(void)
{
    /* Initialize selected GPIO as RTC IO, enable input, disable pullup and pulldown */
    rtc_gpio_init(GPIO_RADAR_INT);
    rtc_gpio_set_direction(GPIO_RADAR_INT, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en(GPIO_RADAR_INT);
    rtc_gpio_pullup_dis(GPIO_RADAR_INT);
    rtc_gpio_hold_en(GPIO_RADAR_INT);

    //enable +5v power for the radar
    rtc_gpio_init(GPIO_VCC5V_EN);
    rtc_gpio_set_direction(GPIO_VCC5V_EN, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(GPIO_VCC5V_EN, 1);
    rtc_gpio_hold_en(GPIO_VCC5V_EN);

    /* Configure the button GPIO as input, enable wakeup */
    gpio_config_t config = {
            .pin_bit_mask = BIT64(GPIO_RADAR_INT),
            .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    gpio_wakeup_enable(GPIO_RADAR_INT, GPIO_INTR_HIGH_LEVEL);
}

static void ulp_gpio_reset(void)
{
    rtc_gpio_hold_dis();
    rtc_gpio_deinit(GPIO_RADAR_INT);
    rtc_gpio_deinit(GPIO_VCC5V_EN);
}

static void ulp_adc_config(void)
{
    ulp_adc_cfg_t adc_cfg = {
        .adc_n    = ADC_UNIT_LDR,
        .channel  = ADC_CHAN_LDR,
        .width    = ADC_WIDTH_LDR,
        .atten    = ADC_ATTEN_LDR,
        .ulp_mode = ADC_ULP_MODE_RISCV,
    };

    ESP_ERROR_CHECK(ulp_adc_init(&adc_cfg));
}

static void ulp_adc_deconfig(void)
{
    //ESP_ERROR_CHECK(ulp_adc_deinit());
}