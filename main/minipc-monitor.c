#include <stdio.h>
#include "esp_log.h"

#include "epd29.h"

#define TAG "main"

void app_main(void) {
  spi_device_handle_t spi = NULL;
  int d = 0;
  epd29_init(&spi);
  epd29_clear(spi, 0xff);
  while (1) {
    for (int i = 0; i < 32; i++) {
      fb[i + 32 * d] = 0x55;
    }
    epd29_frame_sync(spi);
    // epd29_clear(spi, 0x55 + d);
    ESP_LOGI(TAG, "all done, d=%d", d);
    d++;
    if (d >= 64) d = 0;
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}
