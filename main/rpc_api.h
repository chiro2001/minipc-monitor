//
// Created by chiro on 24-2-15.
//

#ifndef MINIPC_MONITOR_RPC_API_H
#define MINIPC_MONITOR_RPC_API_H

#include "rpc_ble.h"
#include "esp_log.h"

#define RPC_FUNC_LEN 48
typedef struct {
  char function[RPC_FUNC_LEN];
  uint8_t args[BLE_FRAME_PAYLOAD_SZ - RPC_FUNC_LEN];
} rpc_request_t;
typedef struct {
  uint8_t type;
  uint8_t length;
  uint8_t *data;
} rpc_request_arg_t;
enum {
  RPC_REQ_TYPE_END = 0,
  RPC_REQ_TYPE_INT,
  RPC_REQ_TYPE_CHANNEL,
};
typedef struct {
  const char *function;
  esp_err_t (*handler)(rpc_request_arg_t *args);
} rpc_table_entry_t;

esp_err_t rpc_req_handler(rpc_request_t *req, size_t payload_len);

#endif //MINIPC_MONITOR_RPC_API_H
