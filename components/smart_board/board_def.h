/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _AUDIO_BOARD_DEFINITION_H_
#define _AUDIO_BOARD_DEFINITION_H_
#include "audio_hal.h"
#include "PinDef.h"
#define BUTTON_VOLUP_ID           -1     /* You need to define the GPIO pins of your board */
#define BUTTON_VOLDOWN_ID         -1     /* You need to define the GPIO pins of your board */
#define BUTTON_MUTE_ID            -1     /* You need to define the GPIO pins of your board */
#define BUTTON_SET_ID             -1     /* You need to define the GPIO pins of your board */
#define BUTTON_MODE_ID            -1     /* You need to define the GPIO pins of your board */
#define BUTTON_PLAY_ID            -1     /* You need to define the GPIO pins of your board */
#define PA_ENABLE_GPIO            GPIO_PA_EN     /* You need to define the GPIO pins of your board */
#define ADC_DETECT_GPIO           -1     /* You need to define the GPIO pins of your board */
#define BATTERY_DETECT_GPIO       -1     /* You need to define the GPIO pins of your board */





/**
 * @brief SDCARD Function Definition
 */
#define FUNC_SDCARD_EN             (0)
#define SDCARD_OPEN_FILE_NUM_MAX    5
#define SDCARD_INTR_GPIO            -1
#define SDCARD_PWR_CTRL             -1

#define ESP_SD_PIN_CLK              -1
#define ESP_SD_PIN_CMD              -1
#define ESP_SD_PIN_D0               -1
#define ESP_SD_PIN_D1               -1
#define ESP_SD_PIN_D2               -1
#define ESP_SD_PIN_D3               -1
#define ESP_SD_PIN_D4               -1
#define ESP_SD_PIN_D5               -1
#define ESP_SD_PIN_D6               -1
#define ESP_SD_PIN_D7               -1
#define ESP_SD_PIN_CD               -1
#define ESP_SD_PIN_WP               -1





//extern audio_hal_func_t AUDIO_NEW_CODEC_DEFAULT_HANDLE;
//extern audio_hal_func_t AUDIO_CODEC_ES8388_DEFAULT_HANDLE;
/**
 * @brief Audio Codec Chip Function Definition
 */
#define FUNC_AUDIO_CODEC_EN       (1)
//#define CODEC_ADC_I2S_PORT        (0)
//#define CODEC_ADC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
//#define CODEC_ADC_SAMPLE_RATE     (48000)
//#define RECORD_HARDWARE_AEC       (false)
#define BOARD_PA_GAIN             (0) /* Power amplifier gain defined by board (dB) */

extern audio_hal_func_t AUDIO_CODEC_ES8311_DEFAULT_HANDLE;

#define AUDIO_CODEC_DEFAULT_CONFIG(){                   \
        .adc_input  = AUDIO_HAL_ADC_INPUT_DIFFERENCE,        \
        .dac_output = AUDIO_HAL_DAC_OUTPUT_LINE1,         \
        .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,        \
        .i2s_iface = {                                  \
            .mode = AUDIO_HAL_MODE_SLAVE,               \
            .fmt = AUDIO_HAL_I2S_NORMAL,                \
            .samples = AUDIO_HAL_16K_SAMPLES,           \
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,        \
        },                                              \
};


//#undef I2S_STREAM_CFG_DEFAULT
//#define I2S_STREAM_CFG_DEFAULT() I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, 16000, I2S_DATA_BIT_WIDTH_16BIT, AUDIO_STREAM_WRITER)

#undef I2S_STREAM_CFG_DEFAULT_WITH_PARA
#define I2S_STREAM_CFG_DEFAULT_WITH_PARA(port, rate, bits, stream_type)  {      \
    .type = stream_type,                                                        \
    .transmit_mode = I2S_COMM_MODE_STD,                                         \
    .chan_cfg = {                                                               \
        .id = port,                                                             \
        .role = I2S_ROLE_MASTER,                                                \
        .dma_desc_num = 3,                                                      \
        .dma_frame_num = 312,                                                   \
        .auto_clear = true,                                                     \
    },                                                                          \
    .std_cfg = {                                                                \
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(rate),                           \
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(bits, I2S_SLOT_MODE_STEREO),  \
        .gpio_cfg = {                                                           \
            .invert_flags = {                                                   \
                .mclk_inv = false,                                              \
                .bclk_inv = false,                                              \
            },                                                                  \
        },                                                                      \
    },                                                                          \
    .expand_src_bits = I2S_DATA_BIT_WIDTH_16BIT,                                \
    .use_alc = false,                                                           \
    .volume = 100,                                                                \
    .out_rb_size = I2S_STREAM_RINGBUFFER_SIZE,                                  \
    .task_stack = I2S_STREAM_TASK_STACK,                                        \
    .task_core = I2S_STREAM_TASK_CORE,                                          \
    .task_prio = I2S_STREAM_TASK_PRIO,                                          \
    .stack_in_ext = false,                                                      \
    .multi_out_num = 0,                                                         \
    .uninstall_drv = true,                                                      \
    .need_expand = false,                                                       \
    .buffer_len = I2S_STREAM_BUF_SIZE,                                          \
}


#endif //_AUDIO_BOARD_DEFINITION_H_
