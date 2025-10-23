#pragma once
#include "driver/rmt_tx.h"

class IRMTransceiver{
private:
    rmt_channel_handle_t m_TxChannel;
    rmt_encoder_handle_t m_NecEncoder;
    rmt_encoder_handle_t m_RawEncoder;

    int prepare();
public:
    int init();
    void deinit();
    int send(uint8_t addr, uint8_t cmd);
    void sendRaw(uint32_t id);
    static void startIRLearn();
};



#define IRM_LED_ON          0xFFFF
#define IRM_LED_OFF         0xFFFE
#define IRM_LED_FLASH       0xFFFD
#define IRM_LED_RED         0xFFF3
#define IRM_LED_GREEN       0xFFF2
#define IRM_LED_BLUE        0xFFF1

#define IRM_TV_ON           0xBFED
