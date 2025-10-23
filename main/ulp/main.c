/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* ULP-RISC-V example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   This code runs on ULP-RISC-V  coprocessor
*/

#include <stdint.h>
#include "ulp_riscv_utils.h"
#include "ulp_riscv_print.h"
#include "ulp_riscv_uart_ulp_core.h"
#include "hal/adc_ll.h"
#include "ulp_riscv_gpio.h"
#include "../include/PinDef.h"
#define TRUE 1
#define FALSE 0
uint32_t adc_threshold = ADC_TRESHOLD_LDR;
int32_t wakeup_result;
uint32_t stop_flag;
//static ulp_riscv_uart_t s_print_uart;

void turnOffLight();
void turnOnLight();
void turnOnLDR();
void turnOffLDR();
int initOnce();

int main (void)
{
    //ulp_riscv_uart_cfg_t cfg = {
    //    .tx_pin = 17,
    //};
    //ulp_riscv_uart_init(&s_print_uart, &cfg);
    //ulp_riscv_print_install((putc_fn_t)ulp_riscv_uart_putc, &s_print_uart);
    //ulp_riscv_print_str("Hello World from ULP-RISCV!\n");
    //ulp_riscv_print_str("Cnt: 0x");
    //ulp_riscv_print_hex(cnt);
    //ulp_riscv_print_str("\n");
    if(stop_flag==TRUE) return 0;

    initOnce();

    
    //check photo resistor to understand it is daytime now
    turnOnLDR();
    ulp_riscv_delay_cycles(20 * ULP_RISCV_CYCLES_PER_MS);
    int32_t ldr_vol = ulp_riscv_adc_read_channel(ADC_UNIT_LDR, ADC_CHAN_LDR);
    //ulp_riscv_delay_cycles(100 * ULP_RISCV_CYCLES_PER_MS);
    //int32_t bat_vol = ulp_riscv_adc1_read_channel(ADC_UNIT_BATTERY, ADC_CHAN_BATTERY);
    //turnOffLDR();
    wakeup_result = ldr_vol;
    //ulp_riscv_wakeup_main_processor();
    if(ldr_vol > adc_threshold) {
        goto fun_end;
    }

    //turn on light
    if(ulp_riscv_gpio_get_level(GPIO_RADAR_INT)==1) {
        turnOnLight();
    }else{
        turnOffLight();
    }

    //keep light on
    while(ulp_riscv_gpio_get_level(GPIO_RADAR_INT)==1 && stop_flag==FALSE) 
    {
        ulp_riscv_delay_cycles(1000 * ULP_RISCV_CYCLES_PER_MS);
    }
    
    //turn off light
    turnOffLight();

fun_end:
    ulp_riscv_gpio_wakeup_clear();
    return 0;
}

int initOnce()
{
    static int done = 0;
    if(done==1) return 0;
    ulp_riscv_gpio_init(GPIO_LEDLIGHT_SW);
    ulp_riscv_gpio_init(GPIO_LDR_PWR);
    done = 1;
    return 0;
}

void turnOffLight()
{
    ulp_riscv_gpio_output_enable(GPIO_LEDLIGHT_SW);
    ulp_riscv_gpio_output_level(GPIO_LEDLIGHT_SW, 0);
}

void turnOnLight()
{
    ulp_riscv_gpio_output_enable(GPIO_LEDLIGHT_SW);
    ulp_riscv_gpio_output_level(GPIO_LEDLIGHT_SW, 1);
}

void turnOnLDR()
{
    ulp_riscv_gpio_output_enable(GPIO_LDR_PWR);
    ulp_riscv_gpio_output_level(GPIO_LDR_PWR, 0);
}

void turnOffLDR()
{
    ulp_riscv_gpio_output_enable(GPIO_LDR_PWR);
    ulp_riscv_gpio_output_level(GPIO_LDR_PWR, 1);
}


