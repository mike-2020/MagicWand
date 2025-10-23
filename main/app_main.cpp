#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "AudioMgr.h"
#include "esp_partition.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "GestureSensor.h"
#include "SampleRecorder.h"
#include "GestureRecognizer.h"
#include "HMIMgr.h"
#include "SoundPlayer.h"
#include "PowerMgr.h"
#include "LedStrip.h"
#include "PinDef.h"
#include "STTService.h"
#include "SpeechAudioCapture.h"
#include "MM_TTSService.h"
#include "MM_LLMService.h"
#include "BD_BaseService.h"
#include "LCDDev.h"
#include "MBaseUI.h"
#include "NetworkMgr.h"
#include "Prophet.h"

static const char *TAG = "APP_MAIN";

extern "C" int usb_disk_init(void);
extern "C" esp_err_t start_file_server(const char *base_path);

// Handle of the wear levelling library instance
wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

esp_err_t mountFilesystem(const char *path, const char *label, const char *opStr) {
    ESP_LOGI(TAG, "Mounting FAT filesystem %s on %s...", label, path);
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formated before
    const esp_vfs_fat_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 4,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    esp_err_t err = ESP_OK;
    if(strcmp(opStr, "rw")==0) {
        err = esp_vfs_fat_spiflash_mount_rw_wl(path, label, &mount_config, &s_wl_handle);
    }else{
        err = esp_vfs_fat_spiflash_mount_ro(path, label, &mount_config);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return err;
    }

    return 0;
}



void unmountFilesystem(const char *path) {
    // Unmount FATFS
    ESP_LOGI(TAG, "Unmounting FAT filesystem");
    ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount_rw_wl(path, s_wl_handle));

    ESP_LOGI(TAG, "Done");
}

//This is to allocate interrupt slot on CPU1
void serviceInitializer(void *ctx)
{
    NetworkMgr::getInstance()->init(false);
    GestureSensor::getInstance()->init();
    LEDStrip::getInstance()->init();
    xTaskNotify((TaskHandle_t)ctx, 0, eNoAction);
    vTaskDelete(NULL);
}


#include "IRMTransceiver.h"
#include "MM_BaseService.h"

extern "C" void app_main(void)
{
    int rc = 0;
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    //install gpio isr service
    gpio_install_isr_service(0);

    //mountFilesystem("/data", "disk", "rw");
    mountFilesystem("/res", "res", "ro");

    PowerMgr::getInstance()->init();
    LCDDev::getInstance()->init();
    MUIMgr::getInstance()->setUI(MUI_SPLASH);

    //start the task on CPU1 in order to allocate interrupt slot on CPU1
    ESP_LOGI(TAG, "Waiting for device initialization for CPU1 core complete...");
    xTaskCreatePinnedToCore(serviceInitializer, "svc_init", 1024*4, xTaskGetCurrentTaskHandle(), 1, NULL, 1);
    //xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, portMAX_DELAY);
    ESP_LOGI(TAG, "Initialization task for CPU1 core has been completed.");

   

    AudioMgr::getInstance()->init();
    //MUIMgr::getInstance()->setUI(MUI_PROPHET);
    //MUIMgr::getInstance()->setUI(MUI_CLOCK);
    vTaskDelay(pdMS_TO_TICKS(12000));
    
    AudioMgr::getInstance()->initStep2();
    HMIMgr::getInstance()->init();
    Prophet::getInstance()->start();
   //LEDStrip::getInstance()->sendCmd(LED_CMD_ON_ALL);
    //PowerMgr::getInstance()->turnOffLEDLight();
    while(1){
        
        vTaskDelay(pdMS_TO_TICKS(5000));
        //LEDStrip::getInstance()->sendCmd(LED_CMD_ON_ALL);
        //PowerMgr::getInstance()->turnOnLEDLight();
        //vTaskDelay(pdMS_TO_TICKS(1000));
        //ESP_LOGI(TAG, "Looping ...");
        //mui->appendText("测试一下");
        printf("free_heap_size = %d\n", esp_get_free_heap_size());
        //static char InfoBuffer[512] = {0}; 
        //vTaskList(InfoBuffer);
		//printf("任务名      任务状态 优先级   剩余栈 任务序号\r\n");
		//printf("\r\n%s\r\n", InfoBuffer);
    }
}

