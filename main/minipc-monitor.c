#include <stdio.h>
#include "esp_log.h"

#include "epd29.h"

#define TAG "main"

void app_main(void) {
  spi_device_handle_t spi = NULL;
  while (1) {
    epd29_init(&spi);
    epd29_display_frame(spi);
    ESP_LOGI(TAG, "all done");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
