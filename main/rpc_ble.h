//
// Created by chiro on 24-2-15.
//

#ifndef MINIPC_MONITOR_BLE_H
#define MINIPC_MONITOR_BLE_H

#include "protocomm.h"
#include "protocomm_ble.h"
#include "protocomm_security0.h"

#define BLE_FRAME_HEADER_SZ (2 + 1 + 1 + 1 + 4 + 1)
#define BLE_FRAME_PAYLOAD_SZ (251 - BLE_FRAME_HEADER_SZ)
typedef struct {
  uint16_t magic;
  uint8_t payload_len;
  uint8_t channel;
  uint32_t channel_offset;
  uint8_t flags;
  uint8_t payload[BLE_FRAME_PAYLOAD_SZ];
} ble_frame_t;
#define BLE_FRAME_MAGIC 0xbeef
enum {
  BLE_FRAME_FLAG_MF = 1 << 0,
};

protocomm_t *start_pc();

void stop_pc(protocomm_t *pc);

#define BLE_CHANNELS_SZ 3
extern uint8_t *ble_channels[BLE_CHANNELS_SZ];

#endif //MINIPC_MONITOR_BLE_H
