//
// Created by chiro on 24-2-15.
//

#ifndef MINIPC_MONITOR_FONTS_H
#define MINIPC_MONITOR_FONTS_H

#include <stdint.h>

#include "esp_err.h"

#define CONV_UNICODE_START 0x4E00
#define CONV_UNICODE_END 0x9FA6
extern const uint8_t unicode2gbk_bin_start[] asm("_binary_unicode2gbk_bin_start");
extern const uint8_t asc12_start[] asm("_binary_ASC12_start");
extern const uint8_t asc16_start[] asm("_binary_ASC16_start");
extern const uint8_t asc48_start[] asm("_binary_ASC48_start");
extern const uint8_t hzk12_start[] asm("_binary_HZK12_start");
// extern const uint8_t hzk16_start[] asm("_binary_HZK16_start");
// extern const uint8_t hzk24_start[] asm("_binary_HZK24S_start");
// extern const uint8_t hzk32_start[] asm("_binary_HZK32_start");

esp_err_t conv_unicode_gbk(uint16_t unicode, uint8_t *gbk);

int conv_utf8_unicode(const uint8_t *utf8, uint16_t *unicode);

int conv_utf8_gbk(const uint8_t *utf8, uint16_t *gbk);

esp_err_t fonts_get_data(const char *utf8, uint8_t font_size, const uint8_t **dst, int *char_len, int *data_len);

inline uint8_t fonts_cjk_width(uint8_t font_size) {
  if (font_size <= 12) return 16;
  return font_size;
}

#endif //MINIPC_MONITOR_FONTS_H
