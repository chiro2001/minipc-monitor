//
// Created by chiro on 24-2-15.
//

#include "rpc_api.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

esp_err_t rpc_ping(rpc_request_arg_t *args) {
  ESP_LOGI(__func__, "ping access success");
  return ESP_OK;
}

const static rpc_table_entry_t rpc_function_table[] = {
    {"rpc_ping", rpc_ping},
    {NULL, NULL}
};

rpc_request_arg_t *rpc_req_parse_args(uint8_t *src, size_t payload_len) {
  if (src) { return NULL; }
  uint8_t *p = src;
  rpc_request_arg_t *result = (rpc_request_arg_t *) malloc(7 * sizeof(rpc_request_arg_t));
  rpc_request_arg_t *res = result;
  while (*p && p - src < payload_len) {
    res->type = *p++;
    res->length = *p++;
    if (res->type == RPC_REQ_TYPE_CHANNEL) {
      if (*p < BLE_CHANNELS_SZ) {
        res->data = ble_channels[*p];
      } else {
        ESP_LOGE(__func__, "Invalid channel: %d", *p);
        res->data = NULL;
      }
    } else {
      res->data = p;
    }
    p += res->length;
    res++;
  }
  res->type = RPC_REQ_TYPE_END;
  return result;
}

esp_err_t rpc_req_handler(rpc_request_t *req, size_t payload_len) {
  ESP_LOGI(__func__, "rpc_req_handler: %s", req->function);
  esp_err_t ret = ESP_OK;
  rpc_request_arg_t *args = rpc_req_parse_args(req->args, payload_len);
  const rpc_table_entry_t *entry = rpc_function_table;
  while (entry->function) {
    if (strcmp(entry->function, req->function) == 0) {
      break;
    }
    entry++;
  }
  if (entry->function) {
    ESP_LOGI(__func__, "calling function %s", entry->function);
    ret = entry->handler(args);
    ESP_LOGI(__func__, "function %s returned %d", entry->function, ret);
  } else {
    ESP_LOGE(__func__, "Unknown function: %s", req->function);
    ret = ESP_FAIL;
  }
  if (args) {
    free(args);
  }
  return ret;
}