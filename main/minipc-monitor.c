#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "epd29.h"
#include "lightbulb.h"
#include "nvs.h"
#include "nvs_flash.h"

#define TAG "main"

// Button - GPIO 1
// Wired RGB - GPIO 5

#define CONFIG_WS2812_LED_NUM 18
#define CONFIG_WS2812_LED_GPIO 5

void rgb_task(void *args) {
  lightbulb_config_t config = {
      //1. 选择 WS2812 输出并进行参数配置
      .type = DRIVER_WS2812,
      .driver_conf.ws2812.led_num = CONFIG_WS2812_LED_NUM,
      .driver_conf.ws2812.ctrl_io = CONFIG_WS2812_LED_GPIO,
      //2. 驱动功能选择，根据你的需要启用/禁用
      .capability.enable_fade = true,
      .capability.fade_time_ms = 800,
      .capability.enable_status_storage = true,
      /* 对于 WS2812 只能选择 LED_BEADS_3CH_RGB */
      .capability.led_beads = LED_BEADS_3CH_RGB,
      .capability.storage_cb = NULL,
      //3. 限制参数，使用细则请参考后面小节
      .external_limit = NULL,
      //4. 颜色校准参数
      .gamma_conf = NULL,
      //5. 初始化照明参数，如果 on 置位将在初始化驱动时点亮球泡灯
      .init_status.mode = WORK_COLOR,
      .init_status.on = true,
      .init_status.hue = 0,
      .init_status.saturation = 100,
      .init_status.value = 100,
  };
  lightbulb_init(&config);
  uint32_t test_case = LIGHTING_RAINBOW | LIGHTING_ALEXA | LIGHTING_COLOR_EFFECT;
  // uint32_t test_case = LIGHTING_WARM_TO_COLD;
  while (1) {
    lightbulb_lighting_output_test(test_case, 2000);
  }
}

void app_main(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  #if 0

  xTaskCreate(rgb_task, "rgb_task", 4096, NULL, 5, NULL);

  spi_device_handle_t spi = NULL;
  int d = 0;
  epd29_init(&spi);
  // epd29_clear(spi, 0xff);
  while (1) {
    for (int i = 0; i < EPD29_WIDTH / 8; i++) {
      for (int j = 0; j < EPD29_HEIGHT; j++) {
        if (d & 1) {
          fb_raw[i + EPD29_WIDTH / 8 * j] = 0x55 + d + j;
        } else {
          fb_raw[i + EPD29_WIDTH / 8 * j] = ~(uint8_t)(0x55 + d + j);
        }
      }
    }
    if (d & 1) {
      epd29_frame_sync_full(spi);
    } else {
      epd29_frame_sync(spi);
    }
    ESP_LOGI(TAG, "all done, d=%d", d);
    d++;
    if (d >= EPD29_HEIGHT) d = 0;
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }

  #elif 0
  spi_device_handle_t spi = NULL;
  epd29_init(&spi);
  // test gray
  while (1) {
    // int level = 16;
    int level = 32;
    for (int k = 0; k < level; k++) {
      epd29_set_depth(spi, level - k);
      memset(fb_raw, 0xff, sizeof(fb_raw));
      for (int i = 0; i < EPD29_WIDTH / 8; i++) {
        for (int j = 0; j < EPD29_HEIGHT * (k + 1) / level; j++) {
          fb_raw[i + EPD29_WIDTH / 8 * j] = 0x00;
        }
      }
      epd29_frame_sync_raw(spi);
    }
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    memset(fb_raw, 0xff, sizeof(fb_raw));
    epd29_frame_sync_full(spi);
  }
  #else
  spi_device_handle_t spi = NULL;
  ESP_LOGI(TAG, "init start");
  epd29_init(&spi);
  ESP_LOGI(TAG, "init color start");
  memset(fb, 0x00, sizeof(fb));
  for (int y = 0; y < EPD29_HEIGHT; y++) {
    uint8_t color = y * 0xff / EPD29_HEIGHT;
    ESP_LOGD(TAG, "y=%d, color=%d", y, color);
    for (int x = 0; x < EPD29_WIDTH; x++) {
      fb[y * EPD29_WIDTH + x] = color;
    }
  }
  // test gray
  while (1) {
    ESP_LOGI(TAG, "sync start");
    epd29_frame_sync(spi);
    ESP_LOGI(TAG, "sync done");
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "init sync full start");
    memset(fb_raw, 0xff, sizeof(fb_raw));
    epd29_frame_sync_full(spi);
  }
  #endif
}
