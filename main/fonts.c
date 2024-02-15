//
// Created by chiro on 24-2-15.
//

#include "fonts.h"

#include <stddef.h>
#include "esp_err.h"
#include "esp_log.h"

esp_err_t conv_unicode_gbk(uint16_t unicode, uint8_t *gbk) {
  // is ascii range
  if (unicode < 0x80) {
    gbk[0] = unicode;
    return ESP_OK;
  }
  if (unicode < CONV_UNICODE_START || unicode > CONV_UNICODE_END) {
    return ESP_FAIL;
  }
  uint32_t offset = (unicode - CONV_UNICODE_START) * 2;
  gbk[0] = unicode2gbk_bin_start[offset];
  gbk[1] = unicode2gbk_bin_start[offset + 1];
  ESP_LOGD(__func__, "unicode=%x, gbk=%x", unicode, *(uint16_t * )(gbk));
  return ESP_OK;
}

int conv_utf8_unicode(const uint8_t *utf8, uint16_t *unicode) {
  if (utf8[0] < 0x80) {
    *unicode = utf8[0];
    return 1;
  }
  if ((utf8[0] & 0xE0) == 0xC0) {
    *unicode = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
    return 2;
  }
  if ((utf8[0] & 0xF0) == 0xE0) {
    *unicode = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
    return 3;
  }
  return -1;
}

int conv_utf8_gbk(const uint8_t *utf8, uint16_t *gbk) {
  uint16_t unicode;
  int len = conv_utf8_unicode(utf8, &unicode);
  ESP_LOGD(__func__, "conv_utf8_unicode len=%d", len);
  if (len < 0) {
    return -1;
  }
  esp_err_t ret = conv_unicode_gbk(unicode, (uint8_t *) gbk);
  if (ret != ESP_OK) {
    return -1;
  }
  return len;
}

esp_err_t fonts_get_data(const char *utf8, uint8_t font_size, const uint8_t **dst, int *char_len, int *data_len) {
  uint16_t gbk;
  int utf8_len = conv_utf8_gbk((const uint8_t *) utf8, &gbk);
  if (utf8_len < 0) {
    ESP_LOGE(__func__, "Invalid utf8: %s", utf8);
    return ESP_FAIL;
  }
  int gbk_len = utf8_len <= 2 ? utf8_len : 2;
  *char_len = utf8_len;
  const uint8_t *base = NULL;
  int length = 0;
  if (gbk_len == 1) {
    // asc
    length = font_size / 2 * font_size / 8;
    switch (font_size) {
      case 12:
        base = asc12_start;
        length = 8 * 12 / 8;
        break;
      case 16:
        base = asc16_start;
        break;
      case 48:
        base = asc48_start;
        break;
      default:
        ESP_LOGE(__func__, "Invalid font size: %d", font_size);
        return ESP_FAIL;
    }
  } else if (gbk_len == 2) {
    // hzk
    length = font_size * font_size / 8;
    switch (font_size) {
      case 12:
        base = hzk12_start;
        length = 24;
        break;
      // case 16:
      //   base = hzk16_start;
      //   break;
      // case 24:
      //   base = hzk24_start;
      //   break;
      // case 32:
      //   base = hzk32_start;
      //   break;
      default:
        ESP_LOGE(__func__, "Invalid font size: %d", font_size);
        return ESP_FAIL;
    }
  } else {
    // invalid length
    ESP_LOGE(__func__, "Invalid gbk length: %d", gbk_len);
    return ESP_FAIL;
  }
  *data_len = length;
  if (gbk_len == 1) {
    *dst = base + ((gbk & 0xff) * length);
  } else {
    *dst = base + ((((gbk >> 8) - 0xa1) * 94 + ((gbk & 0xff) - 0xa1)) * length);
  }
  return ESP_OK;
}