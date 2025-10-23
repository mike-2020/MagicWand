#pragma once
#include "esp_afe_config.h"

#ifdef __cplusplus
extern "C" {
#endif


audio_element_handle_t build_i2s_stream(audio_stream_type_t stream_type);
afe_config_t build_afe_config();
//recorder_sr_handle_t build_recorder_sr(recorder_sr_iface_t **iface);

#ifdef __cplusplus
}
#endif //