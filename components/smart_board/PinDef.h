#pragma once


#define GPIO_RECORDER_BTN       GPIO_NUM_0
#define GPIO_BOOT               GPIO_NUM_0
#define GPIO_POWERON            GPIO_NUM_0

#define GPIO_I2S_SDI            GPIO_NUM_8
#define GPIO_I2S_SCLK           GPIO_NUM_9
#define GPIO_I2S_LRCK           GPIO_NUM_10
#define GPIO_I2S_SDO            GPIO_NUM_11
#define GPIO_I2S_MCLK           GPIO_NUM_17
#define GPIO_PA_EN              GPIO_NUM_39

#define GPIO_IRMTX_PIN          GPIO_NUM_1
#define GPIO_IRMRX_PWR          GPIO_NUM_1
#define GPIO_IRMRX_PIN          GPIO_NUM_3
#define GPIO_VCC5V_EN           GPIO_NUM_5

#define GPIO_IDF_I2C_SCL        GPIO_NUM_41
#define GPIO_IDF_I2C_SDA        GPIO_NUM_42
#define GPIO_ADF_I2C_SCL        GPIO_NUM_20
#define GPIO_ADF_I2C_SDA        GPIO_NUM_21

#define GPIO_MPU_INT            GPIO_NUM_40

//For voltage massurement for Li battery
#define GPIO_BAT_ADC_EN         GPIO_NUM_6
//#define GPIO_BAT_ADC_DET        GPIO_NUM_10
#define ADC_CHAN_BATTERY        ADC_CHANNEL_6   //GPIO_NUM_7
#define ADC_UNIT_BATTERY        ADC_UNIT_1


#define GPIO_LEDSTRIP_EN        GPIO_NUM_46
#define GPIO_LEDSTRIP_DO        GPIO_NUM_45

#define GPIO_LEDLIGHT_SW        GPIO_NUM_2

#define GPIO_RADAR_INT          GPIO_NUM_19

#define ADC_CHAN_LDR            ADC_CHANNEL_3 //GPIO4
#define ADC_UNIT_LDR            ADC_UNIT_1
#define ADC_ATTEN_LDR           ADC_ATTEN_DB_0
#define ADC_WIDTH_LDR          ADC_BITWIDTH_DEFAULT
#define GPIO_LDR_PWR            GPIO_BAT_ADC_EN
/* Set high threshold, approx. 1.75V*/
#define ADC_TRESHOLD_LDR        1000        //enough light if higher than this value

/* For buttons */
#define GPIO_BTN_CENTER         GPIO_NUM_0
#define GPIO_BTN_ADC            GPIO_NUM_18
#define ADC_CHN_BTN             ADC_CHANNEL_3


/* For SPI LCD */
#define GPIO_LCD_PWR_EN         GPIO_NUM_48
#define GPIO_LCD_BL_SW          GPIO_NUM_47
#define GPIO_LCD_SPI_DC         GPIO_NUM_14
#define GPIO_LCD_SPI_SDA        GPIO_NUM_13
#define GPIO_LCD_SPI_SCL        GPIO_NUM_12