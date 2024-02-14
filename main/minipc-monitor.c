#include "settings.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "epd29.h"
#include "lightbulb.h"

// image decoders
#include "jpeg_decoder.h"

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESPS3)
// JPG decoder is on ESP32 rom for this version
#if ESP_IDF_VERSION_MAJOR >= 4  // IDF 4+
#include "esp32/rom/tjpgd.h"
#else  // ESP32 Before IDF 4.0
#include "rom/tjpgd.h"
#endif  // IDF 4+
#else
// use tjpgd.h in component
#include "tjpgd.h"

#endif

#include "pngle.h"

#define TAG "main"

// Button - GPIO 1
// Wired RGB - GPIO 5

#define CONFIG_WS2812_LED_NUM 18
#define CONFIG_WS2812_LED_GPIO 5

spi_device_handle_t spi = NULL;

// JPEG decoder
JDEC jd;
JRESULT rc;
// PNG decoder
pngle_t *pngle = NULL;
uint8_t render_pixel_skip = 0;
// buffers
// uint8_t* source_buf = NULL;       // downloaded image
static uint8_t tjpgd_work[3096];  // tjpgd 3096 is the minimum size
// uint8_t* fb;                      // EPD 2bpp buffer
// uint8_t* bg_img = NULL;           // background image
// static uint32_t feed_buffer_pos = 0;
// opened files
// FILE *fp_downloading = NULL;
FILE *fp_reading = NULL;
// time
// int64_t time_download_start;
// int64_t time_download;
int64_t time_decode;
int64_t time_render;
static const char *jd_errors[] = {
    "Succeeded",
    "Interrupted by output function",
    "Device error or wrong termination of input stream",
    "Insufficient memory pool for the image",
    "Insufficient stream input buffer",
    "Parameter error",
    "Data format error",
    "Right format but not supported",
    "Not supported JPEG standard"};

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

esp_err_t init_flash_storage() {
  ESP_LOGI(TAG, "Initializing SPIFFS");
  esp_vfs_spiffs_conf_t conf = {
      .base_path = storage_base_path,
      .partition_label = storage_partition_label,
      .max_files = 12,
      .format_if_mount_failed = true
  };
  esp_err_t err = esp_vfs_spiffs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(__func__, "Failed to mount SPIFFS (%s)", esp_err_to_name(err));
    return err;
  }
  return ESP_OK;
}

static int tjd_output(
    JDEC *jd,     /* Decompressor object of current session */
    void *bitmap, /* Bitmap data to be output */
    JRECT *rect   /* Rectangular region to output */
) {
  vTaskDelay(0);

  uint32_t w = rect->right - rect->left + 1;
  uint32_t h = rect->bottom - rect->top + 1;
  uint32_t image_width = jd->width;
  uint32_t image_height = jd->height;
  uint8_t *bitmap_ptr = (uint8_t * )
  bitmap;

  // Write to display
  int padding_x;
  int padding_y;
  if (render_pixel_skip > 1) {
    padding_x = ((int) (EPD29_WIDTH) - (int) (image_width) / render_pixel_skip) / 2;
    padding_y = ((int) (EPD29_HEIGHT) - (int) (image_height) / render_pixel_skip) / 2;
  } else {
    padding_x = ((int) (EPD29_WIDTH) - (int) (image_width)) / 2;
    padding_y = ((int) (EPD29_HEIGHT) - (int) (image_height)) / 2;
  }

  // ESP_LOGI(TAG, "tjd_output padding_x=%d padding_y=%d", padding_x, padding_y);

  for (uint32_t i = 0; i < w * h; i++) {
    int xx = rect->left + i % w;
    if (xx < 0 || xx >= image_width) {
      continue;
    }
    int yy = rect->top + i / w;
    if (yy < 0 || yy >= image_height) {
      continue;
    }
    if (render_pixel_skip > 1) {
      xx /= render_pixel_skip;
      yy /= render_pixel_skip;
    }
    uint8_t r = *(bitmap_ptr++);
    uint8_t g = *(bitmap_ptr++);
    uint8_t b = *(bitmap_ptr++);

    // Calculate weighted grayscale
    // uint32_t val = ((r * 30 + g * 59 + b * 11) / 100); // original formula
    uint32_t val = (r * 38 + g * 75 + b * 15) >> 7;  // @vroland recommended formula
    // uint32_t val = r;
    if (render_pixel_skip > 1) {
      epd29_set_pixel(xx + padding_x, yy + padding_y, val);
    } else {
      epd29_set_pixel(xx + padding_x, yy + padding_y, val);
      // epd29_set_pixel(xx + padding_x, yy + padding_y,
      //                 (val / render_pixel_skip) +
      //                 epd29_get_pixel(xx + padding_x, yy + padding_y));
    }
  }

  return 1;
}

static size_t feed_buffer_file(
    JDEC *jd,
    uint8_t *buff,  // Pointer to the read buffer (NULL:skip)
    size_t nd
) {
  assert(fp_reading != NULL);
  uint32_t count = 0;
  if (feof(fp_reading)) {
    // printf("EOF\n");
    return count;
  } else if (!buff) {
    // just move the file pointer
    // printf("skip %d bytes\n", nd);
    fseek(fp_reading, nd, SEEK_CUR);
    count = nd;
  } else {
    // normal read
    count = fread(buff, 1, nd, fp_reading);
    // printf("read %d bytes, got %d\n", nd, count);
  }
  return count;
}

esp_err_t draw_jpeg_file(const char *filename, uint8_t *current_fb) {
  fp_reading = fopen(filename, "rb");
  if (!fp_reading) {
    ESP_LOGE(__func__, "Failed to open file %s for reading", filename);
    goto error;
  }
  rc = jd_prepare(&jd, feed_buffer_file, tjpgd_work, sizeof(tjpgd_work), current_fb);
  if (rc != JDR_OK) {
    ESP_LOGE(__func__, "JPG jd_prepare error: %s", jd_errors[rc]);
    goto error;
  }
  ESP_LOGI("JPG", "width: %d height: %d", jd.width, jd.height);
  // auto set render_pixel_skip
  render_pixel_skip = 0;
  while (jd.width / (render_pixel_skip + 2) > EPD29_WIDTH &&
         jd.height / (render_pixel_skip + 2) > EPD29_HEIGHT) {
    render_pixel_skip++;
  }
  ESP_LOGI("JPG", "render_pixel_skip: %d", render_pixel_skip);

  uint32_t decode_start = esp_timer_get_time();
  vTaskDelay(0);
  rc = jd_decomp(&jd, tjd_output, 0);
  if (rc != JDR_OK) {
    ESP_LOGE(__func__, "JPG jd_decode error: %s", jd_errors[rc]);
    goto error;
  }
  vTaskDelay(0);
  time_decode = (esp_timer_get_time() - decode_start) / 1000;

  ESP_LOGI("decode", "%"
  PRIu32
  " ms . image decompression", time_decode);

  fclose(fp_reading);
  fp_reading = NULL;

  return ESP_OK;
  error:
  fclose(fp_reading);
  fp_reading = NULL;
  return ESP_FAIL;
}

bool str_ends_with(const char *str, const char *patten) {
  int len_str = strlen(str);
  int len_patten = strlen(patten);
  if (len_str < len_patten) {
    return false;
  }
  return strcmp(str + len_str - len_patten, patten) == 0;
}

esp_err_t do_display_images(void) {
  DIR *d;
  struct dirent *dir;
  d = opendir(storage_base_path);
  if (!d) {
    ESP_LOGE(__func__, "unable to load path %s", storage_base_path);
    return ESP_FAIL;
  }
  while ((dir = readdir(d)) != NULL) {
    ESP_LOGI(TAG, "%s", dir->d_name);
    if (str_ends_with(dir->d_name, ".jpg")) {
      // found an image
      ESP_LOGI(TAG, "Found image %s", dir->d_name);
      char full_path[256];
      snprintf(full_path, sizeof(full_path), "%s/%s", storage_base_path, dir->d_name);
      esp_err_t ret = draw_jpeg_file(full_path, fb);
      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Paint image done");
        epd29_clear(spi, 0xff);
        epd29_frame_sync(spi);
        vTaskDelay(30000 / portTICK_PERIOD_MS);
      } else {
        vTaskDelay(1);
      }
    }
  }
  closedir(d);
  return ESP_OK;
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
  #elif 0
  spi_device_handle_t spi = NULL;
  ESP_LOGI(TAG, "init start");
  epd29_init(&spi);
  ESP_LOGI(TAG, "init color start");
  memset(fb, 0x00, sizeof(fb));
  for (int y = 0; y < EPD29_HEIGHT; y++) {
    uint8_t color = y * 0xff / EPD29_HEIGHT;
    ESP_LOGD(TAG, "y=%d, color=%d", y, color);
    for (int x = 0; x < EPD29_WIDTH; x++) {
      fb[y * EPD29_WIDTH + x] = color + x / 3;
    }
  }
  // test gray
  epd29_set_gray_level(32);
  while (1) {
    ESP_LOGI(TAG, "sync start");
    epd29_frame_sync(spi);
    ESP_LOGI(TAG, "sync done");
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "init sync full start");
    memset(fb_raw, 0xff, sizeof(fb_raw));
    epd29_frame_sync_full(spi);
  }
  #else
  // Initializaze Flash Storage
  ESP_ERROR_CHECK(init_flash_storage());

  ESP_LOGI(TAG, "init start");
  epd29_init(&spi);
  ESP_LOGI(TAG, "init color start");

  // epd29_set_gray_level(32);
  // epd29_set_gray_level(8);
  epd29_set_gray_level(16);

  while (true) {
    do_display_images();
  }

  #endif
}
