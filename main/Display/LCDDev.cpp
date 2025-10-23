#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_lcd_panel_vendor.h"   // Dependent header files
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_lcd_gc9a01.h"         // Header file of the target driver component
#include "lv_conf.h"
#include "lvgl.h"
//#include "lv_hal/lv_hal_disp.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "PinDef.h"
#include "LCDDev.h"
#include "PowerMgr.h"


static const char *TAG = "LCDDev";
extern void example_lvgl_demo_ui(lv_disp_t *disp);

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST
#define EXAMPLE_LCD_H_RES              240
#define EXAMPLE_LCD_V_RES              240
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (6 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1

static esp_lcd_panel_handle_t g_panelHandle = NULL;
static SemaphoreHandle_t lvgl_mux = NULL;
static lv_disp_t *g_dispHandle;
static LCDDev g_lcdDevice;


static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t  *display = (lv_display_t  *)user_ctx;
    /* IMPORTANT!!!
     * Inform LVGL that you are ready with the flushing and buf is not used anymore*/
    lv_display_flush_ready(display);
    return false;
}

static void example_lvgl_flush_cb(lv_display_t  *display, const lv_area_t *area, uint8_t * px_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) lv_display_get_user_data(display);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
static void example_lvgl_port_update_callback(lv_display_t  *display)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) lv_display_get_user_data(display);
    return;
#if 0
    switch (display->rotated) {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    }
#endif 
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool LCDDev::lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void LCDDev::unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    LCDDev *handle = (LCDDev*)arg;
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (handle->lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            handle->unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

LCDDev* LCDDev::getInstance()
{
    return &g_lcdDevice;
}

int LCDDev::init()
{
    //static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_display_t * display;      // contains callback functions
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    display = lv_display_create(240, 240);

    PowerMgr::getInstance()->turnOnLCDPWR();

    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << GPIO_LCD_BL_SW,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_LCD_SPI_SDA,
        .miso_io_num = -1,
        .sclk_io_num = GPIO_LCD_SPI_SCL,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = -1,
        .dc_gpio_num = GPIO_LCD_SPI_DC,
        .spi_mode = 0,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = example_notify_lvgl_flush_ready,
        .user_ctx = display,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    /**
    * Used to store the initialization commands and parameters of the LCD driver IC
    */
    // static const gc9a01_lcd_init_cmd_t lcd_init_cmds[] = {
    // //  {cmd, { data }, data_size, delay_ms}
    //     {0xfe, (uint8_t []){0x00}, 0, 0},
    //     {0xef, (uint8_t []){0x00}, 0, 0},
    //     {0xeb, (uint8_t []){0x14}, 1, 0},
    //     ...
    // };

    /* Create the LCD device */

    // const gc9a01_vendor_config_t vendor_config = {  // Used to replace the initialization commands and parameters in the driver component
    //     .init_cmds = lcd_init_cmds,
    //     .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
    // };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,    // Connect the IO number of the LCD reset signal, set to `-1` to indicate not using
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,   // Element order of pixel color (RGB/BGR),
                                                    // Usually controlled by the command `LCD_CMD_MADCTL (36h)`
        .bits_per_pixel = 16,  // Bit depth of the color format (RGB565: 16, RGB666: 18),
                                                    // usually controlled by the command `LCD_CMD_COLMOD (3Ah)`
        // .vendor_config = &vendor_config,           // Used to replace the initialization commands and parameters in the driver component
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &g_panelHandle));

    /* Initialize the LCD device */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(g_panelHandle, true));   // Use these functions as needed
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(g_panelHandle, true, false));
    // ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(g_panelHandle, true));
    // ESP_ERROR_CHECK(esp_lcd_panel_set_gap(g_panelHandle, 0, 0));
    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panelHandle, true));


    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(GPIO_LCD_BL_SW, 1);


    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    int buffLen = EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t);
    lv_color_t *buf1 = (lv_color_t*)heap_caps_malloc(buffLen, MALLOC_CAP_SPIRAM);
    assert(buf1);
    lv_color_t *buf2 = (lv_color_t*)heap_caps_malloc(buffLen, MALLOC_CAP_SPIRAM);
    assert(buf2);
    // initialize LVGL draw buffers
    //lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 20);

    lv_display_set_buffers(display, buf1, buf2, buffLen, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);
    lv_display_set_user_data(display, g_panelHandle);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, this, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    return 0;
}

int LCDDev::deinit()
{
    esp_lcd_panel_del(g_panelHandle);
    return 0;
}