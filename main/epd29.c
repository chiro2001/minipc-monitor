#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "epd29.h"

#define TAG "epd92"

#if defined(CONFIG_IDF_TARGET_ESP8266)
#define EPD29_PIN_BUSY 5
#define EPD29_PIN_RST  2
#define EPD29_PIN_DC   4
#define EPD29_PIN_CS   15
#define EPD29_PIN_SCK  14
#define EPD29_PIN_SDA  13
#define EPD29_SPI_HOST HSPI_HOST
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
#define EPD29_PIN_BUSY 3
#define EPD29_PIN_RST  8
#define EPD29_PIN_DC   10
#define EPD29_PIN_CS   7
#define EPD29_PIN_SCK  4
#define EPD29_PIN_SDA  6
#define EPD29_SPI_HOST SPI2_HOST
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define EPD29_PIN_BUSY 2
#define EPD29_PIN_RST  3
#define EPD29_PIN_DC   10
#define EPD29_PIN_CS   6
#define EPD29_PIN_SCK  7
#define EPD29_PIN_SDA  11
#define EPD29_SPI_HOST SPI2_HOST
#endif

uint8_t fb_raw[EPD29_WIDTH * EPD29_HEIGHT / 8] = {0};
uint8_t fb[EPD29_WIDTH * EPD29_HEIGHT] = {0};
bool first_display = true;
bool in_part_mode = false;
// static uint8_t gray_level = 32;
// static uint8_t gray_level = 16;
static uint8_t gray_level = 8;

#define EPD29_SOFT_SPI 1

#define reorder_bits(x)  ( \
  (((x >> 0) & 1) << 7) | \
  (((x >> 1) & 1) << 6) | \
  (((x >> 2) & 1) << 5) | \
  (((x >> 3) & 1) << 4) | \
  (((x >> 4) & 1) << 3) | \
  (((x >> 5) & 1) << 2) | \
  (((x >> 6) & 1) << 1) | \
  (((x >> 7) & 1) << 0)    \
)

#if EPD29_SOFT_SPI

void ep29_soft_spi_byte_lsb(uint8_t data) {
  for (int i = 0; i < 8; i++) {
    gpio_set_level(EPD29_PIN_SCK, 0);
    gpio_set_level(EPD29_PIN_SDA, (data & 0x01) ? 1 : 0);
    data >>= 1;
    gpio_set_level(EPD29_PIN_SCK, 1);
  }
}

void ep29_soft_spi_byte_msb(uint8_t data) {
  for (int i = 0; i < 8; i++) {
    gpio_set_level(EPD29_PIN_SCK, 0);
    gpio_set_level(EPD29_PIN_SDA, (data & 0x80) ? 1 : 0);
    data <<= 1;
    gpio_set_level(EPD29_PIN_SCK, 1);
  }
}

// #define ep29_soft_spi_byte ep29_soft_spi_byte_lsb
#define ep29_soft_spi_byte ep29_soft_spi_byte_msb

esp_err_t epd29_data(spi_device_handle_t spi, const uint8_t *data, const size_t len) {
  gpio_set_level(EPD29_PIN_CS, 0);
  gpio_set_level(EPD29_PIN_DC, 1);
  for (size_t i = 0; i < len; i++) {
    ep29_soft_spi_byte(data[i]);
  }
  gpio_set_level(EPD29_PIN_CS, 1);
  return ESP_OK;
}

esp_err_t epd29_data_value(spi_device_handle_t spi, const uint8_t data, const size_t len) {
  gpio_set_level(EPD29_PIN_CS, 0);
  gpio_set_level(EPD29_PIN_DC, 1);
  for (size_t i = 0; i < len; i++) {
    ep29_soft_spi_byte(data);
  }
  gpio_set_level(EPD29_PIN_CS, 1);
  return ESP_OK;
}

esp_err_t epd29_cmd(spi_device_handle_t spi, const uint8_t cmd) {
  gpio_set_level(EPD29_PIN_CS, 0);
  gpio_set_level(EPD29_PIN_DC, 0);
  ep29_soft_spi_byte(cmd);
  gpio_set_level(EPD29_PIN_CS, 1);
  gpio_set_level(EPD29_PIN_DC, 1);
  return ESP_OK;
}

#else

#if 0

static uint8_t spi_convert_buf[512] = {0};
static uint8_t spi_convert_cmd_buf;

esp_err_t epd29_data(spi_device_handle_t spi, const uint8_t *data, const size_t len) {
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  for (size_t i = 0; i < len; i++) {
    spi_convert_buf[i] = reorder_bits(data[i]);
  }
  t.length = len * 8;
  t.tx_buffer = spi_convert_buf;
  t.user = (void *) 1; // set DC to 1
  ret = spi_device_polling_transmit(spi, &t);
  assert(ret == ESP_OK);
  return ret;
}

esp_err_t epd29_cmd(spi_device_handle_t spi, const uint8_t cmd) {
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  spi_convert_cmd_buf = reorder_bits(cmd);
  t.length = 8;
  t.tx_buffer = &spi_convert_cmd_buf;
  t.user = (void *) 0; // set DC to 0
  ret = spi_device_polling_transmit(spi, &t);
  assert(ret == ESP_OK);
  return ret;
}

#else

esp_err_t epd29_data(spi_device_handle_t spi, const uint8_t *data, const size_t len) {
  ESP_LOGD(TAG, "epd29_data(data=%p, len=%d)", data, len);
  const size_t batch = 512;
  esp_err_t ret = ESP_OK;
  size_t sz = batch;
  if (sz > len) {
    sz = len;
  }
  size_t pos = 0;
  while (pos < len) {
    ESP_LOGD(TAG, "spi transmit pos 0x%x", pos);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = sz * 8;
    t.tx_buffer = data + pos;
    t.user = (void *) 1; // set DC to 1
    // if (pos + sz < len) {
    //   t.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
    // }
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
    pos += sz;
    if (pos == len) {
      break;
    }
    if (pos + batch > len) {
      sz = len - pos;
    } else {
      sz = batch;
    }
  }
  return ret;
}

esp_err_t epd29_data_value(spi_device_handle_t spi, const uint8_t data, const size_t len) {
  const size_t batch = 512;
  uint8_t buffer[batch];
  memset(buffer, data, batch);
  if (batch >= len) {
    return epd29_data(spi, buffer, len);
  }
  esp_err_t ret = ESP_OK;
  size_t sz = batch;
  size_t pos = 0;
  while (pos < len) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = sz * 8;
    t.tx_buffer = buffer;
    t.user = (void *) 1; // set DC to 1
    // if (pos + sz < len) {
    //   t.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
    // }
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
    pos += sz;
    if (pos == len) {
      break;
    }
    if (pos + batch > len) {
      sz = len - pos;
    } else {
      sz = batch;
    }
  }
  return ret;
}

esp_err_t epd29_cmd(spi_device_handle_t spi, const uint8_t cmd) {
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 8;
  t.tx_buffer = &cmd;
  t.user = (void *) 0; // set DC to 0
  ret = spi_device_polling_transmit(spi, &t);
  assert(ret == ESP_OK);
  return ret;
}

#endif

#endif

esp_err_t epd29_cmd_data(spi_device_handle_t spi, const uint8_t cmd, const uint8_t *data, const size_t len) {
  esp_err_t ret;
  ret = epd29_cmd(spi, cmd);
  assert(ret == ESP_OK);
  if (data && len) {
    ret = epd29_data(spi, data, len);
  }
  return ret;
}

esp_err_t epd29_cmd_data1(spi_device_handle_t spi, const uint8_t cmd, const uint8_t data) {
  return epd29_cmd_data(spi, cmd, &data, 1);
}

void epd29_spi_pre_transfer_callback(spi_transaction_t *t) {
  gpio_set_level(EPD29_PIN_DC, (int) t->user);
}

void epd29_reset() {
  // reset
  // ESP_LOGI(TAG, "reset");
  gpio_set_level(EPD29_PIN_RST, 1);
  vTaskDelay(20 / portTICK_PERIOD_MS);
  gpio_set_level(EPD29_PIN_RST, 0);
  vTaskDelay(150 / portTICK_PERIOD_MS);
  gpio_set_level(EPD29_PIN_RST, 1);
  // vTaskDelay(100 / portTICK_PERIOD_MS);
}

esp_err_t epd29_wait_until_idle(void) {
  vTaskDelay(10 / portTICK_PERIOD_MS);
  while (gpio_get_level(EPD29_PIN_BUSY) == EPD29_BUSY_VAL) {
    vTaskDelay(1);
    // ESP_LOGD(TAG, "waiting busy...");
  }
  return ESP_OK;
}

#if defined(EPD_CHIP_SSD1680)
esp_err_t epd29_init_full(spi_device_handle_t spi) {
  epd29_reset();
  epd29_wait_until_idle();
  epd29_cmd(spi, EPD_CMD_SW_RESET);
  epd29_wait_until_idle();
  {
    uint16_t epd_height = EPD29_HEIGHT;
    uint8_t data[3] = {
        (epd_height - 1) & 0xFF,
        ((epd_height - 1) >> 8) & 0xFF,
        0x00,
    };
    epd29_cmd_data(spi, EPD_CMD_DRIVER_OUTPUT_CONTROL, data, sizeof(data));
  }
  {
    uint8_t data[3] = {0xd7, 0xd6, 0x9d};
    epd29_cmd_data(spi, EPD_CMD_BOOSTER_SOFT_START_CONTROL, data, sizeof(data));
  }
  epd29_cmd_data1(spi, EPD_CMD_WRITE_VCOM_REGISTER, 0xa8);
  epd29_cmd_data1(spi, EPD_CMD_SET_DUMMY_LINE_PERIOD, 0x1a);
  epd29_cmd_data1(spi, EPD_CMD_DATA_ENTRY_MODE_SETTING, 0x03);
  epd29_set_depth(spi, 0);
  epd29_wait_until_idle();
  return ESP_OK;
}

esp_err_t epd29_display_frame(spi_device_handle_t spi) {
  epd29_cmd_data1(spi, EPD_CMD_DISPLAY_UPDATE_CONTROL_2, 0xc4);
  epd29_cmd(spi, EPD_CMD_MASTER_ACTIVATION);
  epd29_wait_until_idle();
  return ESP_OK;
}

esp_err_t epd29_set_lut(spi_device_handle_t spi, const uint8_t *lut, uint8_t depth) {
  if (depth == 0) {
    return epd29_cmd_data(spi, EPD_CMD_WRITE_LUT_REGISTER, lut, 30);
  } else {
    esp_err_t ret;
    ret = epd29_cmd(spi, EPD_CMD_WRITE_LUT_REGISTER);
    if (ret != ESP_OK) {
      return ret;
    }
    for (int i = 0; i < 30; i++) {
      if (i >= 20 && i <= 22) {
        uint8_t idx = (depth - 1) * 3 + i - 20;
        ret = epd29_data(spi, lut_gray_tp + idx, 1);
      } else {
        ret = epd29_data(spi, lut + i, 1);
      }
      if (ret != ESP_OK) {
        return ret;
      }
    }
    return ret;
  }
}

esp_err_t epd29_set_depth(spi_device_handle_t spi, uint8_t depth) {
  if (depth > 2) {
    epd29_cmd_data1(spi, EPD_CMD_SET_GATE_TIME, 0x05);
  } else if (depth == 2) {
    epd29_cmd_data1(spi, EPD_CMD_SET_GATE_TIME, 0x02);
  } else {
    epd29_cmd_data1(spi, EPD_CMD_SET_GATE_TIME, 0x08);
  }
  if (depth > 0) {
    epd29_set_lut(spi, lut_fast, depth);
  } else {
    epd29_set_lut(spi, lut_slow, depth);
  }
  return ESP_OK;
}
#elif defined(EPD_CHIP_UC8151D)

esp_err_t epd29_init_full(spi_device_handle_t spi) {
  epd29_reset();
  epd29_wait_until_idle();
  epd29_cmd_data1(spi, EPD_CMD_PANEL_SETTING, 0x1f);
  {
    uint8_t data[3] = {
        EPD29_WIDTH,
        EPD29_HEIGHT >> 8,
        EPD29_HEIGHT & 0xff
    };
    epd29_cmd_data(spi, EPD_CMD_RESOLUTION_SETTING, data, sizeof(data));
  }
  epd29_cmd_data1(spi, EPD_CMD_VCOM_INTERVAL, 0x97);
  epd29_cmd(spi, EPD_CMD_POWER_ON);
  epd29_wait_until_idle();
  in_part_mode = false;
  return ESP_OK;
}

uint8_t epd29_level_to_phase(uint8_t depth) {
  if (gray_level == 16) {
    const static uint8_t table[17] = {
        32, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 6, 8, 10, 12, 16
    };
    return table[depth < 17 ? depth : 0];
  } else if (gray_level == 32) {
    const static uint8_t table[33] = {
        32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 16, 18
    };
    return table[depth < 33 ? depth : 0];
  } else if (gray_level == 8) {
    const static uint8_t table[9] = {
        32, 2, 2, 2, 4, 4, 10, 24, 42
    };
    return table[depth < 9 ? depth : 0];
  } else {
    return 32;
  }
}

esp_err_t epd29_set_lut(spi_device_handle_t spi, const uint8_t *_lut, uint8_t depth) {
  ESP_LOGD(TAG, "epd29_set_lut depth=%d", depth);
  uint8_t tmp[sizeof(lut_part_vcom)] = {0};
  if (depth == 0) {
    epd29_cmd_data(spi, 0x20, lut_part_vcom, sizeof(lut_part_vcom));
    epd29_cmd_data(spi, 0x21, lut_part_ww, sizeof(lut_part_ww));
    epd29_cmd_data(spi, 0x22, lut_part_bw, sizeof(lut_part_bw));
    epd29_cmd_data(spi, 0x23, lut_part_wb, sizeof(lut_part_wb));
    epd29_cmd_data(spi, 0x24, lut_part_bb, sizeof(lut_part_bb));
  } else {
    memcpy(tmp, lut_part_vcom, sizeof(lut_part_vcom));
    tmp[1] = epd29_level_to_phase(depth);
    epd29_cmd_data(spi, 0x20, tmp, sizeof(lut_part_vcom));
    memcpy(tmp, lut_part_ww, sizeof(lut_part_ww));
    tmp[1] = epd29_level_to_phase(depth);
    epd29_cmd_data(spi, 0x21, tmp, sizeof(lut_part_ww));
    memcpy(tmp, lut_part_bw, sizeof(lut_part_bw));
    tmp[1] = epd29_level_to_phase(depth);
    epd29_cmd_data(spi, 0x22, tmp, sizeof(lut_part_bw));
    memcpy(tmp, lut_part_wb, sizeof(lut_part_wb));
    tmp[1] = epd29_level_to_phase(depth);
    epd29_cmd_data(spi, 0x23, tmp, sizeof(lut_part_wb));
    memcpy(tmp, lut_part_bb, sizeof(lut_part_bb));
    tmp[1] = epd29_level_to_phase(depth);
    epd29_cmd_data(spi, 0x24, tmp, sizeof(lut_part_bb));
  }
  epd29_wait_until_idle();
  return ESP_OK;
}

esp_err_t epd29_set_depth(spi_device_handle_t spi, uint8_t depth) {
  epd29_init_part(spi);
  epd29_set_lut(spi, NULL, depth);
  return ESP_OK;
}

esp_err_t epd29_init_part(spi_device_handle_t spi) {
  epd29_reset();
  epd29_wait_until_idle();
  epd29_cmd_data1(spi, EPD_CMD_PANEL_SETTING, 0xbf);
  epd29_cmd_data1(spi, EPD_CMD_VCOM_DC_SETTING, 0x08);
  {
    uint8_t data[3] = {
        EPD29_WIDTH,
        EPD29_HEIGHT >> 8,
        EPD29_HEIGHT & 0xff
    };
    epd29_cmd_data(spi, EPD_CMD_RESOLUTION_SETTING, data, sizeof(data));
  }
  epd29_cmd_data1(spi, EPD_CMD_VCOM_INTERVAL, 0x17);
  epd29_set_lut(spi, NULL, 0);
  epd29_cmd(spi, EPD_CMD_POWER_ON);
  epd29_wait_until_idle();
  in_part_mode = true;
  return ESP_OK;
}

esp_err_t epd29_display_frame(spi_device_handle_t spi) {
  epd29_cmd(spi, EPD_CMD_DISPLAY_REFRESH);
  epd29_wait_until_idle();
  first_display = false;
  return ESP_OK;
}

void epd29_set_partial_window(spi_device_handle_t spi, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint16_t xe = (x + w - 1) | 0x0007; // byte boundary inclusive (last byte)
  uint16_t ye = y + h - 1;
  uint8_t data[7] = {
      x & 0xff, xe & 0xff,
      y >> 8, y & 0xff,
      ye >> 8, ye & 0xff,
      0x01
  };
  epd29_cmd_data(spi, EPD_CMD_PARTIAL_WINDOW, data, sizeof(data));
}

esp_err_t epd29_fill_value(spi_device_handle_t spi, uint8_t command, uint8_t value) {
  epd29_cmd(spi, command);
  epd29_data_value(spi, value, EPD29_WIDTH * EPD29_HEIGHT / 8);
  return ESP_OK;
}

esp_err_t epd29_clear(spi_device_handle_t spi, uint8_t color) {
  // if (first_display) {
  //   epd29_init_full(spi);
  //   epd29_fill_value(spi, EPD_CMD_START_TRAINS1, color);
  //   epd29_fill_value(spi, EPD_CMD_START_TRAINS2, color);
  //   epd29_display_frame(spi);
  //   return ESP_OK;
  // }
  // if (!in_part_mode) {
  //   epd29_init_part(spi);
  // }
  // epd29_fill_value(spi, EPD_CMD_START_TRAINS2, color);
  // epd29_display_frame(spi);

  memset(fb_raw, color, sizeof(fb_raw));
  epd29_frame_sync_full(spi);
  return ESP_OK;
}

esp_err_t epd29_frame_sync_full(spi_device_handle_t spi) {
  if (in_part_mode) {
    epd29_init_full(spi);
  }
  epd29_cmd_data(spi, EPD_CMD_START_TRAINS1, fb_raw, sizeof(fb_raw));
  epd29_cmd_data(spi, EPD_CMD_START_TRAINS2, fb_raw, sizeof(fb_raw));
  epd29_display_frame(spi);
  return ESP_OK;
}

esp_err_t epd29_frame_sync_raw(spi_device_handle_t spi) {
  int64_t start_time = esp_timer_get_time();
  if (!in_part_mode) {
    epd29_init_part(spi);
  }
  epd29_cmd(spi, EPD_CMD_PARTIAL_IN);
  epd29_set_partial_window(spi, 0, 0, EPD29_WIDTH, EPD29_HEIGHT);

  uint8_t command;
  if (first_display) {
    command = EPD_CMD_START_TRAINS1;
  } else {
    command = EPD_CMD_START_TRAINS2;
  }
  epd29_cmd(spi, command);
  epd29_data(spi, fb_raw, sizeof(fb_raw));

  epd29_cmd(spi, EPD_CMD_PARTIAL_OUT);
  epd29_display_frame(spi);
  ESP_LOGI(TAG, "draw time %d ms", (int) ((esp_timer_get_time() - start_time) / 1000));
  return ESP_OK;
}

bool epd29_convert(uint8_t depth) {
  bool skip = true;
  uint8_t cmp = depth * 0xfflu / gray_level;
  for (size_t y = 0; y < EPD29_HEIGHT; y++) {
    ESP_LOGD(TAG, "[y=%d] color=%d, cmp=%d, r=%d",
             y, fb[y * EPD29_WIDTH], cmp, fb[y * EPD29_WIDTH] < cmp);
    for (size_t x = 0; x < EPD29_WIDTH; x += 8) {
      uint8_t b = 0;
      for (size_t i = 0; i < 8; i++) {
        uint8_t c = fb[x + i + y * EPD29_WIDTH];
        if (c < cmp) {
          b |= 1 << (7 - i);
        }
      }
      b = ~b;
      fb_raw[x / 8 + y * EPD29_WIDTH / 8] = b;
      if (skip && b != 0xff) {
        skip = false;
      }
    }
  }
  return skip;
}

esp_err_t epd29_frame_sync(spi_device_handle_t spi) {
  int64_t start_time = esp_timer_get_time();
  for (int k = 0; k < gray_level; k++) {
    bool skip = epd29_convert(k);
    if (!skip) {
      epd29_set_depth(spi, gray_level - k);
      epd29_frame_sync_raw(spi);
      ESP_LOGD(TAG, "gray level %d done", k);
    } else {
      ESP_LOGI(TAG, "gray level %d skipped", k);
    }
  }
  ESP_LOGI(TAG, "frame draw time %d ms", (int) ((esp_timer_get_time() - start_time) / 1000));
  return ESP_OK;
}

#endif

esp_err_t epd29_init(spi_device_handle_t *spi) {
  esp_err_t ret = ESP_OK;
  // setup gpio
  {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << EPD29_PIN_RST) | (1ULL << EPD29_PIN_DC);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = true;
    gpio_config(&io_conf);
  }
  {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << EPD29_PIN_BUSY);
    io_conf.mode = GPIO_MODE_INPUT;
    #if (EPD29_BUSY_VAL)
    io_conf.pull_down_en = true;
    #else
    io_conf.pull_up_en = true;
    #endif
    gpio_config(&io_conf);
  }
#if EPD29_SOFT_SPI
  {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask =
        (1ULL << EPD29_PIN_CS) | (1ULL << EPD29_PIN_SCK) | (1ULL << EPD29_PIN_SDA);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = true;
    gpio_config(&io_conf);
  }
#else
  // init spi
  if (*spi == NULL) {
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = EPD29_PIN_SDA,
        .sclk_io_num = EPD29_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 24000000,
        .spics_io_num = EPD29_PIN_CS,
        .queue_size = 7,
        .pre_cb = epd29_spi_pre_transfer_callback,
    };
    ret = spi_bus_initialize(EPD29_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(EPD29_SPI_HOST, &devcfg, spi);
    ESP_ERROR_CHECK(ret);
  }
#endif

  first_display = true;
  ret = epd29_init_full(*spi);
  // ret = epd29_init_part(*spi);
  ESP_ERROR_CHECK(ret);

  memset(fb_raw, 0xff, sizeof(fb_raw));
  memset(fb, 0xff, sizeof(fb));
  // epd29_clear(*spi, 0xff);
  epd29_frame_sync_full(*spi);
  return ret;
}

void epd29_set_gray_level(uint8_t gray_level_) {
  gray_level = gray_level_;
}

inline void epd29_set_pixel(int x, int y, uint8_t color) {
  if (x < 0 || x >= EPD29_WIDTH) return;
  if (y < 0 || y >= EPD29_HEIGHT) return;
  // ESP_LOGI(TAG, "epd29_set_pixel(%d, %d, color=%x)", x, y, color);
  fb[y * EPD29_WIDTH + x] = color;
}

inline uint8_t epd29_get_pixel(int x, int y) {
  if (x < 0 || x >= EPD29_WIDTH) return 0x00;
  if (y < 0 || y >= EPD29_HEIGHT) return 0x00;
  return fb[y * EPD29_WIDTH + x];
}