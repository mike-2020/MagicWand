#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "IRMNECEncoder.h"
#include "IRMTransceiver.h"
#include "PinDef.h"
#include "ir_learn.h"
#include "ir_encoder.h"
#include "PowerMgr.h"
#include "ir_db.h"


#define EXAMPLE_IR_RESOLUTION_HZ     1000000 // 1MHz resolution, 1 tick = 1us

static const char *TAG = "IRM_TRANSCEIVER";
static rmt_transmit_config_t transmit_config;
static IRMTransceiver g_irmTransceiver;
static ir_learn_handle_t g_irLearnHandle;

int IRMTransceiver::init()
{
    ESP_LOGI(TAG, "install IR NEC encoder");
    ir_nec_encoder_config_t nec_encoder_cfg = {
        .resolution = EXAMPLE_IR_RESOLUTION_HZ,
    };
    ir_encoder_config_t ir_encoder_cfg = {
        .resolution = EXAMPLE_IR_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &m_NecEncoder));
    ESP_ERROR_CHECK(ir_encoder_new(&ir_encoder_cfg, &m_RawEncoder));

    // this example won't send NEC frames in a loop
    transmit_config = {
        .loop_count = 0, // no loop
    };

    return prepare();
}

int IRMTransceiver::prepare()
{
    ESP_LOGI(TAG, "create RMT TX channel");
    rmt_tx_channel_config_t tx_channel_cfg = {
        .gpio_num = GPIO_IRMTX_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
        .mem_block_symbols = 128, // amount of RMT symbols that the channel can store at a time
        .trans_queue_depth = 4,  // number of transactions that allowed to pending in the background, this example won't queue multiple transactions, so queue depth > 1 is sufficient
    };
    
    //tx_channel_cfg.flags.with_dma = true;

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &m_TxChannel));

    ESP_LOGI(TAG, "modulate carrier to TX channel");
    rmt_carrier_config_t carrier_cfg = {
        .frequency_hz = 38000, // 38KHz
        .duty_cycle = 0.33,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(m_TxChannel, &carrier_cfg));

    ESP_LOGI(TAG, "enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(m_TxChannel));
    return 0;
}

void IRMTransceiver::deinit()
{
    ESP_ERROR_CHECK(rmt_disable(m_TxChannel));
    rmt_del_channel(m_TxChannel);
    PowerMgr::getInstance()->turnOffIRMPWR();  //IRM_TX may leave IRM_PWR to ON state. It will cause current leakage.
}

int IRMTransceiver::send(uint8_t addr, uint8_t cmd)
{
    ir_nec_scan_code_t scan_code = {
        .address = (uint16_t) (((uint16_t)addr << 8) | ((~addr) & 0xFF)), //0x0440,
        .command = (uint16_t) (((uint16_t)cmd << 8) | ((~cmd) & 0xFF)), //0x3003,
    };
    ESP_LOGI(TAG, "addr = 0x%X, cmd = 0x%X.", scan_code.address, scan_code.command);
    init();
    ESP_ERROR_CHECK(rmt_transmit(m_TxChannel, m_NecEncoder, &scan_code, sizeof(scan_code), &transmit_config));
    deinit();
    return 0;
}

void IRMTransceiver::sendRaw(uint32_t id)
{
    
    int num = sizeof(IRMCODE_XIAOMI_AC_TURNON)/sizeof(rmt_symbol_word_t);
    rmt_symbol_word_t *data = IRMCODE_XIAOMI_AC_TURNON;
    ESP_LOGI(TAG, "num=%d", num);
    
    init();
    ESP_ERROR_CHECK(rmt_transmit(m_TxChannel, m_RawEncoder, data, num, &transmit_config));
    rmt_tx_wait_all_done(m_TxChannel, -1);  // wait all transactions finished
    deinit();
}

void saveDB(uint32_t id, struct ir_learn_sub_list_head *data)
{
    struct ir_learn_sub_list_t *sub_it;
    SLIST_FOREACH(sub_it, data, next) {
        rmt_symbol_word_t *rmt_symbols = sub_it->symbols.received_symbols;
        size_t symbol_num = sub_it->symbols.num_symbols;

    }
}

esp_err_t ir_learn_print_cheader(struct ir_learn_sub_list_head *cmd_list)
{
    //IR_LEARN_CHECK(cmd_list, "list pointer can't be NULL!", ESP_ERR_INVALID_ARG);

    uint8_t sub_num = 0;
    struct ir_learn_sub_list_t *sub_it;
    rmt_symbol_word_t *p_symbols;

    SLIST_FOREACH(sub_it, cmd_list, next) {
        ESP_LOGI(TAG, "sub_it:[%d], timediff:%03d ms, symbols:%03d",
                 sub_num++,
                 sub_it->timediff / 1000,
                 sub_it->symbols.num_symbols);
        printf("\n\n");
        p_symbols = sub_it->symbols.received_symbols;
        for (int i = 0; i < sub_it->symbols.num_symbols; i++) {
            printf("{%04d, %d, %04d, %d}, \r\n",
                   p_symbols->duration0, p_symbols->level0, p_symbols->duration1, p_symbols->level1);
            p_symbols++;
        }
        printf("\n\n");
    }
    return ESP_OK;
}

 void ir_learn_auto_learn_cb(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data)
 {

    switch (state) {
        /**< IR learn ready, after successful initialization */
        case IR_LEARN_STATE_READY:
            ESP_LOGI(TAG, "IR Learn ready");
            break;
        /**< IR learn exit */
        case IR_LEARN_STATE_EXIT:
            ESP_LOGI(TAG, "IR Learn exit");
            PowerMgr::getInstance()->turnOffIRMPWR();
            break;
        /**< IR learn successfully */
        case IR_LEARN_STATE_END:
            ESP_LOGI(TAG, "IR Learn end");
            //ir_learn_save_result(&ir_test_result, data);
            ir_learn_print_cheader(data);
            ir_learn_stop(&g_irLearnHandle);
            break;
        /**< IR learn failure */
        case IR_LEARN_STATE_FAIL:
            ESP_LOGI(TAG, "IR Learn failed, retry");
            break;
        /**< IR learn step, start from 1 */
        case IR_LEARN_STATE_STEP:
        default:
            ESP_LOGI(TAG, "IR Learn step:[%d][%d]", state, sub_step);
            break;
    }
    return;
}


void IRMTransceiver::startIRLearn()
{
    const ir_learn_cfg_t config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,/*!< RMT clock source */
        .resolution = 1000000,         /*!< RMT resolution, 1M, 1 tick = 1us*/
        .learn_count = 4,              /*!< IR learn count needed */
        .learn_gpio = GPIO_IRMRX_PIN,     /*!< IR learn io that consumed by the sensor */
        .callback = ir_learn_auto_learn_cb,       /*!< IR learn result callback for user */

        .task_priority = 5,   /*!< IR learn task priority */
        .task_stack = 1024*4,   /*!< IR learn task stack size */
        .task_affinity = 1,   /*!< IR learn task pinned to core (-1 is no affinity) */

    };

    PowerMgr::getInstance()->turnOnIRMPWR();
    ESP_ERROR_CHECK(ir_learn_new(&config, &g_irLearnHandle));
}