#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#define ASR_VOICEPRINT_NONE     -1
#define ASR_VOICEPRINT_BABA     0

static const char *TAG = "VoicePrint";

static int asr_voiceprint_registration_mode = ASR_VOICEPRINT_NONE;
static int asr_voiceprint_repeat_times = 0;
long long int asr_voiceprint_registration_start_time = 0;

#define ASR_VP_BUFFER_SIZE  16*1024*2
int16_t *asr_vp_input_buffer = NULL;
int asr_vp_input_buffer_len = 0;
int asr_vp_input_data_len = 0;

void asr_voiceprint_buffer_reset()
{
    asr_vp_input_buffer_len = ASR_VP_BUFFER_SIZE;
    asr_vp_input_data_len = 0;
}


void asr_set_voiceprint_registration_mode(int val)
{
    if(val == ASR_VOICEPRINT_NONE && asr_voiceprint_registration_mode!=ASR_VOICEPRINT_NONE)
    {
        //speaker_enroll_clean();
        asr_voiceprint_registration_start_time = 0;
    }
    if(asr_voiceprint_registration_mode!=ASR_VOICEPRINT_NONE && val != asr_voiceprint_registration_mode)
    {
        //speaker_enroll_clean();
        asr_voiceprint_registration_start_time = 0;
    }
    asr_voiceprint_registration_mode = val;
    asr_voiceprint_registration_start_time = esp_timer_get_time();
    asr_voiceprint_buffer_reset();
}

int asr_get_voiceprint_registration_mode()
{
    ESP_LOGI(TAG, "asr_voiceprint_registration_mode=%d", asr_voiceprint_registration_mode);
    if(asr_voiceprint_registration_mode!=ASR_VOICEPRINT_NONE && (asr_voiceprint_registration_start_time != 0 && esp_timer_get_time() - asr_voiceprint_registration_start_time > 3*60*1000*1000LL)) {
        asr_set_voiceprint_registration_mode(ASR_VOICEPRINT_NONE);
    }
    ESP_LOGI(TAG, "asr_voiceprint_registration_mode2=%d", asr_voiceprint_registration_mode);
    if(asr_voiceprint_registration_mode!=ASR_VOICEPRINT_NONE) {
        asr_voiceprint_registration_start_time = esp_timer_get_time();
    }
    return asr_voiceprint_registration_mode;
}

int asr_voiceprint_buffer_init()
{
    if(asr_vp_input_buffer==NULL) {
        asr_vp_input_buffer = (int16_t*)malloc(ASR_VP_BUFFER_SIZE);
        asr_vp_input_buffer_len = ASR_VP_BUFFER_SIZE;
        asr_vp_input_data_len   = 0;
        size_t mem = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGI(TAG, "Free memory left: %d.", mem);
    }
    return ESP_OK;
}
int asr_voiceprint_buffer_append(int16_t *data, int len)
{

    if(asr_vp_input_buffer == NULL || asr_vp_input_buffer_len < len) {
        return ESP_FAIL;
    } 
    
    memcpy(asr_vp_input_buffer + asr_vp_input_data_len, data, len);
    asr_vp_input_buffer_len -= len;
    asr_vp_input_data_len += len;
    return ESP_OK;
}


