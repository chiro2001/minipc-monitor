#include "settings.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "rpc_api.h"
#include "epd29.h"
#include "lightbulb.h"
#include "fonts.h"

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
uint8_t render_pixel_skip = 1;
uint8_t render_pixel_skip_off = 0;
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
    padding_x = ((int) (epd29_get_window_width()) - (int) (image_width) / render_pixel_skip) / 2;
    padding_y = ((int) (epd29_get_window_height()) - (int) (image_height) / render_pixel_skip) / 2;
  } else {
    padding_x = ((int) (epd29_get_window_width()) - (int) (image_width)) / 2;
    padding_y = ((int) (epd29_get_window_height()) - (int) (image_height)) / 2;
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
    epd29_set_pixel(xx + padding_x, yy + padding_y, val);
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
  render_pixel_skip = 1;
  while (jd.width / (render_pixel_skip + render_pixel_skip_off) > epd29_get_window_width() ||
         jd.height / (render_pixel_skip + render_pixel_skip_off) > epd29_get_window_height()) {
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

  ESP_LOGI("decode", "%ld ms . image decompression", time_decode);

  if (fp_reading) {
    fclose(fp_reading);
  }
  fp_reading = NULL;

  return ESP_OK;
  error:
  if (fp_reading) {
    fclose(fp_reading);
  }
  fp_reading = NULL;
  return ESP_FAIL;
}

void on_draw_png(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
  uint32_t r = rgba[0]; // 0 - 255
  uint32_t g = rgba[1]; // 0 - 255
  uint32_t b = rgba[2]; // 0 - 255
  uint32_t a = rgba[3]; // 0: fully transparent, 255: fully opaque

  int image_width = (int) pngle_get_width(pngle);
  int image_height = (int) pngle_get_height(pngle);
  int epd_width = epd29_get_window_width();
  int epd_height = epd29_get_window_height();

  if (render_pixel_skip == 0xff) {
    render_pixel_skip = 1;
    while (image_width / (render_pixel_skip + render_pixel_skip_off) > epd_width ||
           image_height / (render_pixel_skip + render_pixel_skip_off) > epd_height) {
      render_pixel_skip++;
    }
    ESP_LOGI("PNG", "render_pixel_skip: %d", render_pixel_skip);
    return;
  }
  if (render_pixel_skip > 1) {
    image_width = image_width / render_pixel_skip;
    image_height = image_height / render_pixel_skip;
  }

  int padding_x = (epd_width - image_width) / 2;
  int padding_y = (epd_height - image_height) / 2;

  // if (a == 0) {
  //     // skip transparent pixels
  //     return;
  // }

  uint32_t val = (r * 38 + g * 75 + b * 15) >> 7;  // @vroland recommended formula
  // use alpha in white background
  val = (a == 0) ? 255 : val;
  // val = (val * 256 / (256 - a)) & 0xff;
  // uint8_t color = gamme_curve[val];
  uint8_t color = val;

  // // print info
  // static int cnt = 0;
  // if (cnt % 100 == 0)
  //   ESP_LOGI("PNG", "x: %d y: %d w: %d h: %d r: %d g: %d b: %d a: %d val: %d px: %d py: %d color: %x",
  //            x, y, w, h, r, g, b, a, val, padding_x, padding_y, color);
  // cnt++;

  if (render_pixel_skip == 0) {
    for (uint32_t yy = 0; yy < h; yy++) {
      for (uint32_t xx = 0; xx < w; xx++) {
        epd29_set_pixel(xx + x + padding_x,
                        yy + y + padding_y,
                        color);
      }
    }
  } else {
    for (uint32_t yy = 0; yy < h; yy++) {
      for (uint32_t xx = 0; xx < w; xx++) {
        int xxx = xx + x;
        int yyy = yy + y;
        // if (xxx % render_pixel_skip != 0 || yyy % render_pixel_skip != 0) {
        //   continue;
        // }
        epd29_set_pixel(xxx / render_pixel_skip + padding_x,
                        yyy / render_pixel_skip + padding_y,
                        color);
      }
    }
  }
  vTaskDelay(0);
}

esp_err_t draw_png_file(const char *filename, uint8_t *current_fb) {
  fp_reading = fopen(filename, "rb");
  if (!fp_reading) {
    ESP_LOGE(__func__, "Failed to open file %s for reading", filename);
    goto error;
  }
  int r = 0;
  uint32_t decode_start = esp_timer_get_time();
  if (pngle != NULL) {
    pngle_destroy(pngle);
    pngle = NULL;
  }
  pngle = pngle_new();
  render_pixel_skip = 0xff;
  pngle_set_user_data(pngle, current_fb);
  pngle_set_draw_callback(pngle, on_draw_png);

  uint8_t buf[1024];
  while (!feof(fp_reading)) {
    size_t bytes_read = fread(buf, 1, sizeof(buf), fp_reading);
    r = pngle_feed(pngle, buf, bytes_read);
    if (r < 0) {
      ESP_LOGE(__func__, "PNG pngle_feed error: %d %s", r, pngle_error(pngle));
      goto error;
    }
  }
  time_decode = (esp_timer_get_time() - decode_start) / 1000;
  ESP_LOGI("PNG", "width: %d height: %d", pngle_get_width(pngle), pngle_get_height(pngle));
  ESP_LOGI("decode", "%ld ms . image decompression", time_decode);
  fclose(fp_reading);
  fp_reading = NULL;
  return ESP_OK;
  error:
  if (fp_reading) {
    fclose(fp_reading);
    fp_reading = NULL;
  }
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
  char full_path[256];
  int cnt = 0;
  bool success = true;
  while ((dir = readdir(d)) != NULL) {
    ESP_LOGI(TAG, "%s", dir->d_name);
    if (success) {
      if (cnt & 1) {
        epd29_set_window(epd29_width() / 2, 0, epd29_width() / 2, epd29_height());
      } else {
        epd29_set_window(0, 0, epd29_width() / 2, epd29_height());
      }
      cnt++;
    }
    if (str_ends_with(dir->d_name, ".jpg")) {
      // found an image
      ESP_LOGI(TAG, "Found jpg image %s", dir->d_name);
      memset(fb, 0xff, sizeof(fb));
      snprintf(full_path, sizeof(full_path), "%s/%s", storage_base_path, dir->d_name);
      esp_err_t ret = draw_jpeg_file(full_path, fb);
      success = ret == ESP_OK;
    } else if (str_ends_with(dir->d_name, ".png")) {
      // found an image
      ESP_LOGI(TAG, "Found png image %s", dir->d_name);
      memset(fb, 0xff, sizeof(fb));
      snprintf(full_path, sizeof(full_path), "%s/%s", storage_base_path, dir->d_name);
      esp_err_t ret = draw_png_file(full_path, fb);
      success = ret == ESP_OK;
    } else {
      success = false;
    }
    if (success) {
      ESP_LOGI(TAG, "Paint image done");
      epd29_clear(spi, 0xff);
      epd29_frame_sync(spi);
      vTaskDelay(3000 / portTICK_PERIOD_MS);
    } else {
      vTaskDelay(1);
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
    for (int i = 0; i < epd29_width() / 8; i++) {
      for (int j = 0; j < epd29_height(); j++) {
        if (d & 1) {
          fb_raw[i + epd29_width() / 8 * j] = 0x55 + d + j;
        } else {
          fb_raw[i + epd29_width() / 8 * j] = ~(uint8_t)(0x55 + d + j);
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
    if (d >= epd29_height()) d = 0;
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
      for (int i = 0; i < epd29_width() / 8; i++) {
        for (int j = 0; j < epd29_height() * (k + 1) / level; j++) {
          fb_raw[i + epd29_width() / 8 * j] = 0x00;
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
  memset(fb, 0xff, sizeof(fb));
  for (int y = 0; y < epd29_width(); y++) {
    uint8_t color = y * 0xff / epd29_width();
    for (int x = 0; x < epd29_height(); x++) {
      epd29_set_pixel(x, y, color);
    }
  }
  // test gray
  // epd29_set_gray_level(32);
  epd29_set_gray_level(8);
  epd29_init_part(spi);
  while (1) {
    ESP_LOGI(TAG, "sync start");
    epd29_frame_sync(spi);
    ESP_LOGI(TAG, "sync done");
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "init sync full start");
    memset(fb_raw, 0xff, sizeof(fb_raw));
    epd29_frame_sync_full(spi);
  }
  #elif 0
  // Initializaze Flash Storage
  ESP_ERROR_CHECK(init_flash_storage());

  ESP_LOGI(TAG, "init");
  epd29_init(&spi);
  ESP_LOGI(TAG, "init config");

  epd29_set_dir(EPD29_DIR_LANDSCAPE);
  epd29_set_gray_level(32);
  // epd29_set_gray_level(8);
  // epd29_set_gray_level(16);

  ESP_LOGI(TAG, "loop start");
  // epd29_set_window(epd29_width() / 2, 0, epd29_width() / 2, epd29_height());
  // epd29_set_window(epd29_width() / 2, epd29_width() / 2, epd29_height());
  // epd29_set_window(48, 0, epd29_height(), epd29_height());

  while (true) {
    do_display_images();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
  #elif 0
  start_pc();
  ESP_LOGI(TAG, "start pc done");
  #else
  // test fonts
  ESP_LOGI(TAG, "init");
  epd29_init(&spi);
  ESP_LOGI(TAG, "init config");

  // epd29_set_dir(EPD29_DIR_LANDSCAPE);
  epd29_set_dir(EPD29_DIR_PORTRAIT);
  epd29_set_gray_level(0);
  // const char *str = "测试文本";
  const char *str = "all_in 奥利安";
  const char *p = str;
  const uint8_t *data = NULL;
  int char_len = 0, data_len = 0;
  int x = 0, y = 0;
  // uint8_t fonts_size[] = {12, 16, 24, 32};
  uint8_t fonts_size[] = {12};
  uint8_t fonts_size_cnt = 0;
  esp_err_t r;
  while (true) {
    uint8_t font_size = fonts_size[fonts_size_cnt];
    do {
      int64_t time_start = esp_timer_get_time();
      r = fonts_get_data(p, font_size, &data, &char_len, &data_len);
      if (r != ESP_OK) {
        ESP_LOGE(TAG, "failed");
        break;
      }
      uint8_t font_width = fonts_cjk_width(font_size);
      uint8_t font_height = font_size;
      if (char_len == 1) font_width /= 2;
      if (data) {
        for (int i = 0; i < font_width; i++) {
          for (int j = 0; j < font_height; j++) {
            uint8_t color = (data[(j * font_width + i) / 8] & (0x80 >> (i % 8))) ? 0x00 : 0xff;;
            epd29_set_pixel(i + x, j + y, color);
          }
        }
      }
      char buf[32];
      memcpy(buf, p, char_len);
      buf[char_len] = '\0';
      ESP_LOGD(TAG, "font_size=%d, char_len=%d, data_len=%d, char=%s, time=%ld us",
               font_size, char_len, data_len, buf, (esp_timer_get_time() - time_start));
      if (char_len == 1) {
        x += font_width <= 8 ? 6 : font_width;
      } else {
        x += font_size <= 12 ? (font_size + 1) : font_size;
      }
      p += char_len;
    } while (*p);
    epd29_frame_sync(spi);

    x = 0;
    y += font_size <= 12 ? (font_size + 1) : font_size;
    if (y >= epd29_get_window_height()) {
      y = 0;
      memset(fb, 0xff, sizeof(fb));
      epd29_clear(spi, 0xff);
    }
    fonts_size_cnt = (fonts_size_cnt + 1) % sizeof(fonts_size);
    p = str;
    // vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  #endif
}
