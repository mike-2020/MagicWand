#include "esp_log.h"
#include "audio_pipeline.h"
#include "audio_recorder.h"
#include "recorder_sr.h"
#include "i2s_stream.h"
#include "esp_afe_config.h"

audio_element_handle_t build_i2s_stream(audio_stream_type_t stream_type)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, 16000, I2S_DATA_BIT_WIDTH_16BIT, stream_type);
    i2s_cfg.type = stream_type;
    //i2s_cfg.use_alc= true;
    //i2s_cfg.volume = 100;
    if(stream_type==AUDIO_STREAM_WRITER) {
        i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    }else{
        i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;   //this param may be overwritten by SoundPlayer. Change AUDIO_MIC_CHAN_NUM instead.
        i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = 16000;
    }
    return i2s_stream_init(&i2s_cfg);
}

afe_config_t build_afe_config()
{
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    return afe_config;
}

recorder_sr_handle_t build_recorder_sr(recorder_sr_iface_t **iface)
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.wakenet_init = true;
    recorder_sr_cfg.afe_cfg.vad_mode = VAD_MODE_4;
    recorder_sr_cfg.multinet_init = true;
    recorder_sr_cfg.mn_language = ESP_MN_CHINESE;
    recorder_sr_cfg.afe_cfg.aec_init = false;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;
    recorder_sr_cfg.afe_cfg.pcm_config.mic_num = 1;
    recorder_sr_cfg.afe_cfg.pcm_config.ref_num = 0;
    recorder_sr_cfg.afe_cfg.pcm_config.total_ch_num = 1;
    //recorder_sr_cfg.input_order[0] = DAT_CH_0;
    //recorder_sr_cfg.input_order[1] = DAT_CH_1;

    return recorder_sr_create(&recorder_sr_cfg, iface);
}