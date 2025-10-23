#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
//#include "audio_element.h"
//#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "SoundPlayer.h"
#include "AudioMgr.h"
#include "ASRService.h"
#include "esp_peripherals.h"
#include "board.h"
#include "SpeechAudioCapture.h"
#include "STTService.h"

static const char *TAG = "AUDIO_MGR";
static AudioMgr g_audioMgr;
static audio_board_handle_t board_handle;
static int volume_level = 90;

AudioMgr* AudioMgr::getInstance()
{
    return &g_audioMgr;
}

int AudioMgr::init(void)
{

    esp_log_level_set("*", ESP_LOG_VERBOSE);
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGI(TAG, "[1.0] Init Peripheral Set");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[2.0] Start codec chip");
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, volume_level);

    SoundPlayer::getInstance()->init();
    SoundPlayer::getInstance()->speak("WELCOME", true);
    //speaker_verification_init();
   
    return 0;
}

int AudioMgr::initStep2()
{
    ASRService::getInstance()->init();
    return 0;
}

void AudioMgr::setVolume(int vol)
{
    if(vol > 100) vol = 100;
    if(vol < 0) vol = 0;
    audio_hal_set_volume(board_handle->audio_hal, vol);
}

void AudioMgr::deinit()
{
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_STOP);
    audio_board_deinit(board_handle);
}