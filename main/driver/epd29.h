#ifndef __EPD29_H__
#define __EPD29_H__

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

// #define EPD_CHIP_SSD1680 1
#define EPD_CHIP_UC8151D 1

#if defined(EPD_CHIP_SSD1680)
// CHIP: SSD1680
// Display commands
#define EPD_CMD_DRIVER_OUTPUT_CONTROL 0x01
#define EPD_CMD_GATE_VOLTAGE_CONTROL 0x03
#define EPD_CMD_SOURCE_VOLTAGE_CONTROL 0x04
#define EPD_CMD_BOOSTER_SOFT_START_CONTROL 0x0C
// #define EPD_CMD_GATE_SCAN_START_POSITION 0x0F
#define EPD_CMD_DEEP_SLEEP_MODE 0x10
#define EPD_CMD_DATA_ENTRY_MODE_SETTING 0x11
#define EPD_CMD_SW_RESET 0x12
// #define EPD_CMD_TEMPERATURE_SENSOR_CONTROL 0x1A
#define EPD_CMD_MASTER_ACTIVATION 0x20
#define EPD_CMD_DISPLAY_UPDATE_CONTROL_1 0x21
#define EPD_CMD_DISPLAY_UPDATE_CONTROL_2 0x22
#define EPD_CMD_WRITE_RAM 0x24
#define EPD_CMD_WRITE_VCOM_REGISTER 0x2C
#define EPD_CMD_WRITE_LUT_REGISTER 0x32
#define EPD_CMD_SET_DUMMY_LINE_PERIOD 0x3A
#define EPD_CMD_SET_GATE_TIME 0x3B
// #define EPD_CMD_BORDER_WAVEFORM_CONTROL 0x3C
#define EPD_CMD_SET_RAM_X_ADDRESS_START_END_POSITION 0x44
#define EPD_CMD_SET_RAM_Y_ADDRESS_START_END_POSITION 0x45
#define EPD_CMD_SET_RAM_X_ADDRESS_COUNTER 0x4E
#define EPD_CMD_SET_RAM_Y_ADDRESS_COUNTER 0x4F
#define EPD_CMD_TERMINATE_FRAME_READ_WRITE 0xFF
const static uint8_t lut_fast[30] = {
    0b00010000, 0b00011000, 0b00011000, 0b00001000,
    0b00011000, 0b00011000, 0b00001000, 0b00000000,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x13, 0x14,
    0x44, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00
};
const static uint8_t lut_slow[30] = {
    0x50, 0xAA, 0x55, 0xAA,
    0x11, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF,
    0x1F, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00
};
const static uint8_t lut_gray_tp[(16 - 1) * 3] = {
    0x05, 0x10, 0x00,
    0x0a, 0x10, 0x00,
    0x10, 0x10, 0x11,
    0x18, 0x10, 0x11,
    0x20, 0x10, 0x22,
    0x28, 0x20, 0x22,
    0x30, 0x20, 0x23,
    0x40, 0x20, 0x24,
    0x48, 0x20, 0x34,
    0x50, 0x20, 0x34,
    0x60, 0x20, 0x44,
    0x68, 0x31, 0x54,
    0x70, 0x32, 0x64,
    0x78, 0x43, 0x74,
    0x80, 0x45, 0x94
};
#define EPD29_BUSY_VAL 1
#elif defined(EPD_CHIP_UC8151D)
// CHIP: UC8151D
#define EPD_CMD_PANEL_SETTING 0x00
#define EPD_CMD_POWER_SETTING 0x01
#define EPD_CMD_POWER_OFF 0x02
#define EPD_CMD_POWER_OFF_SEQ 0x03
#define EPD_CMD_POWER_ON 0x04
#define EPD_CMD_POWER_ON_ME 0x05
#define EPD_CMD_BOOSTER_SOFT_START 0x06
#define EPD_CMD_DEEP_SLEEP 0x07
#define EPD_CMD_START_TRAINS1 0x10
#define EPD_CMD_DATA_STOP 0x11
#define EPD_CMD_DISPLAY_REFRESH 0x12
#define EPD_CMD_START_TRAINS2 0x13
#define EPD_CMD_AUTO_SEQ 0x17
#define EPD_CMD_LUT_OPTION 0x2a
#define EPD_CMD_PLL_OPTION 0x30
#define EPD_CMD_TEMP_CALI 0x40
#define EPD_CMD_TEMP_SELECT 0x41
#define EPD_CMD_TEMP_WRITE 0x42
#define EPD_CMD_TEMP_READ 0x43
#define EPD_CMD_PANEL_BREAK_CHECK 0x44
#define EPD_CMD_VCOM_INTERVAL 0x50
#define EPD_CMD_LOW_POWER_DETECTION 0x51
#define EPD_CMD_TCOM_SETTING 0x60
#define EPD_CMD_RESOLUTION_SETTING 0x61
#define EPD_CMD_GATE_SOURCE_SETTING 0x65
#define EPD_CMD_REVISION 0x70
#define EPD_CMD_GET_STATUS 0x71
#define EPD_CMD_AUTO_MEASURE_VCOM 0x80
#define EPD_CMD_READ_VCOM_VALUE 0x81
#define EPD_CMD_VCOM_DC_SETTING 0x82
#define EPD_CMD_PARTIAL_WINDOW 0x90
#define EPD_CMD_PARTIAL_IN 0x91
#define EPD_CMD_PARTIAL_OUT 0x92
#define EPD_CMD_PROGRAM_MODE 0xa0
#define EPD_CMD_ACTIVE_PROGRAM 0xa1
#define EPD_CMD_READ_OTP 0xa2
#define EPD_CMD_CASCADE_SETTING 0xe0
#define EPD_CMD_POWER_SAVING 0xe3
#define EPD_CMD_LVD_VOLTAGE 0xe4
#define EPD_CMD_FORCE_TEMP 0xe5
//partial screen update LUT
//#define Tx19 0x19 // original value is 25 (phase length)
#define Tx19 0x20   // new value for test is 32 (phase length)
const static uint8_t lut_part_vcom[] = {
    0x00, Tx19, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
};
const static uint8_t lut_part_ww[] = {
    0x00, Tx19, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const static uint8_t lut_part_bw[] = {
    0x80, Tx19, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const static uint8_t lut_part_wb[] = {
    0x40, Tx19, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const static uint8_t lut_part_bb[] = {
    0x00, Tx19, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
#define EPD29_BUSY_VAL 0
#endif

esp_err_t epd29_data(spi_device_handle_t spi, const uint8_t *data, const size_t len);

esp_err_t epd29_cmd(spi_device_handle_t spi, const uint8_t cmd);

esp_err_t epd29_cmd_data(spi_device_handle_t spi, const uint8_t cmd, const uint8_t *data, const size_t len);

void epd29_spi_pre_transfer_callback(spi_transaction_t *t);

void epd29_reset();

esp_err_t epd29_wait_until_idle(void);

esp_err_t epd29_set_lut(spi_device_handle_t spi, const uint8_t *lut, uint8_t depth);

esp_err_t epd29_set_depth(spi_device_handle_t spi, uint8_t depth);

esp_err_t epd29_init_full(spi_device_handle_t spi);

esp_err_t epd29_init(spi_device_handle_t *spi);

esp_err_t epd29_display_frame(spi_device_handle_t spi);

esp_err_t epd29_frame_sync(spi_device_handle_t spi);

esp_err_t epd29_clear(spi_device_handle_t spi, uint8_t color);

#define EPD29_WIDTH 128
#define EPD29_HEIGHT 296

extern uint8_t fb[EPD29_WIDTH * EPD29_HEIGHT / 8];

#endif  // __EPD29_H__