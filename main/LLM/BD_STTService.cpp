
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "BD_STTService.h"


static const char *BD_URL_STT = "https://vop.baidu.com/pro_api";


int BD_STTService::processAudioData()
{
    int rc = 0;
    int len = 2048;
    char *m_pAudioDataBuffer;
    rc = rb_read(m_ringBufferHandle, m_pAudioDataBuffer, 2048, portMAX_DELAY);
    if(rc>0) {

    }
}

void BD_STTService::proc(void *ctx)
{
    BD_STTService *handle = (BD_STTService*)ctx;

    while(1) {

    }
}


int BD_STTService::start()
{
    m_ringBufferHandle = rb_create(512, 16);

}

