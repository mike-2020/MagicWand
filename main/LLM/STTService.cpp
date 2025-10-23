#include <string.h>
#include "esp_log.h"
#include <speex/speex.h>
#include "raw_stream.h"
#include "STTService.h"

#define FRAME_SIZE   160
#define PACKAGE_SIZE 64
static const char *TAG = "STTService";
static void *g_speexEncoderState = NULL;
static SpeexBits g_speexBits;

int STTService::init()
{
    m_nEncodingFrameOccupiedLen = 0;
    m_nEncodingFrameLen = 0;
    m_pEncodingFrameBuffer = NULL;

    // Create a new encoder state in narrowband mode
    g_speexEncoderState = speex_encoder_init(&speex_wb_mode);

    // Set quality to 7
    uint32_t tmp = STT_SPEECH_SPEEX_QUALITY;
    speex_encoder_ctl(g_speexEncoderState, SPEEX_SET_QUALITY, &tmp);
    speex_encoder_ctl(g_speexEncoderState, SPEEX_GET_FRAME_SIZE, &m_nEncodingFrameLen);
    m_nEncodingFrameLen *= sizeof(int16_t);
    ESP_LOGW(TAG, "speex frame size: %u bytes.", m_nEncodingFrameLen);
    m_pEncodingFrameBuffer = (char*)malloc(m_nEncodingFrameLen);
    //speex_bits_nbytes(&bits)
    // Initialization of the structure that holds the bits
    speex_bits_init(&g_speexBits);
    return ESP_OK;
}

int STTService::deinit()
{
    if(g_speexEncoderState==NULL) return ESP_FAIL;
    // Destroy the decoder state
    speex_encoder_destroy(g_speexEncoderState);
    g_speexEncoderState = NULL;
    // Destroy the bit-stream struct
    speex_bits_destroy(&g_speexBits);
    if(m_pEncodingFrameBuffer!=NULL) {
        free(m_pEncodingFrameBuffer);
        m_pEncodingFrameBuffer = NULL;
    }
    return 0;
}

int STTService::encodeSpeex(char *pInBuffer, int nInLen, ringbuf_handle_t rawStream)
{
    int nInputDataLeftLen = nInLen; //the amount of the consumed data from pBuffer
    char outBuffer[80];
    int rc = 0;
    while(1) {
        int nCopyLen = (m_nEncodingFrameLen - m_nEncodingFrameOccupiedLen) > nInputDataLeftLen ? nInputDataLeftLen : (m_nEncodingFrameLen - m_nEncodingFrameOccupiedLen);
        memcpy(m_pEncodingFrameBuffer + m_nEncodingFrameOccupiedLen, pInBuffer + (nInLen - nInputDataLeftLen), nCopyLen);
        nInputDataLeftLen -= nCopyLen;
        m_nEncodingFrameOccupiedLen += nCopyLen;

        //no enough data to fill frame buffer. break to get more speech data.
        if(m_nEncodingFrameOccupiedLen < m_nEncodingFrameLen) break;
        if(m_nEncodingFrameOccupiedLen != m_nEncodingFrameLen) {
            ESP_LOGE(TAG, "There must be something wrong.");
        }

        //Frame buffer is full for Speex encoding
        // Flush all the bits in the struct so we can encode a new frame
        speex_bits_reset(&g_speexBits);
        // Encode frame
        speex_encode_int(g_speexEncoderState, (int16_t*)m_pEncodingFrameBuffer, &g_speexBits);
        // Copy the bits to an array of char that can be written
        size_t n = speex_bits_write(&g_speexBits, outBuffer, sizeof(outBuffer));
        ESP_LOGI(TAG, "speex output len: %d.", n);

        //write encoded data to raw stream
        rc = rb_write(rawStream, outBuffer, n, pdMS_TO_TICKS(30));
        if(rc!=n) {
            ESP_LOGW(TAG, "Failed to write data (n = %d, rc = %d).", n, rc);
        }

        m_nEncodingFrameOccupiedLen = 0;  //the data in frame buffer has been consumed, so reset m_nEncodingFrameOccupiedLen to zero
    }

    return ESP_OK;
}

void STTService::resetEncoding()
{
    m_nEncodingFrameOccupiedLen = 0;
}