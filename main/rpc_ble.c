//
// Created by chiro on 24-2-15.
//

#include "rpc_ble.h"

#include <string.h>
#include "esp_log.h"
#include "epd29.h"
#include "rpc_api.h"

#define TAG "rpc_ble"

uint8_t *ble_channels[] = {
    NULL,   // command channel
    fb,     // frame buffer channel
    fb_raw, // temp frame buffer channel
};
size_t ble_channels_len[] = {
    0, // command channel
    sizeof(fb), // frame buffer channel
    sizeof(fb_raw), // temp frame buffer channel
};
uint8_t ble_frame_payload_buffer[2048] = {0};
size_t ble_frame_payload_buffer_offset = 0;

esp_err_t req_handler(uint32_t session_id,
                      const uint8_t *inbuf, ssize_t inlen,
                      uint8_t **outbuf, ssize_t *outlen,
                      void *priv_data) {
  *outbuf = NULL;
  *outlen = 0;
  ESP_LOGI(TAG, "test_req_handler inlen=%d, outlen=%d", inlen, *outlen);
  if (inlen <= BLE_FRAME_HEADER_SZ) {
    ESP_LOGE(TAG, "Invalid ble header sz");
    return ESP_FAIL;
  }
  ble_frame_t *frame = (ble_frame_t *) inbuf;
  if (frame->magic != BLE_FRAME_MAGIC) {
    ESP_LOGE(TAG, "Invalid ble header magic number");
    return ESP_FAIL;
  }
  if (frame->channel >= BLE_CHANNELS_SZ) {
    ESP_LOGE(TAG, "Invalid ble header channel");
    return ESP_FAIL;
  }
  uint8_t *channel = ble_channels[frame->channel];
  if (channel) {
    // write to channel
    size_t wr_len = frame->payload_len;
    if (wr_len + frame->channel_offset > ble_channels_len[frame->channel]) {
      ESP_LOGE(TAG, "Invalid ble header channel_offset");
      return ESP_FAIL;
    }
    memcpy(channel + frame->channel_offset, frame->payload, wr_len);
    ESP_LOGI(TAG, "write to channel %d, offset %d, len %d", frame->channel, frame->channel_offset, wr_len);
    return ESP_OK;
  } else {
    // command mode
    if (frame->flags & BLE_FRAME_FLAG_MF) {
      // more flag
      ESP_LOGI(TAG, "more frame, save to buffer");
      memcpy(ble_frame_payload_buffer + ble_frame_payload_buffer_offset, frame->payload, frame->payload_len);
      ble_frame_payload_buffer_offset += frame->payload_len;
      return ESP_OK;
    }
    uint8_t *payload = frame->payload;
    size_t payload_len = frame->payload_len;
    if (ble_frame_payload_buffer_offset > 0) {
      // append to buffer
      memcpy(ble_frame_payload_buffer + ble_frame_payload_buffer_offset, frame->payload, frame->payload_len);
      ble_frame_payload_buffer_offset += frame->payload_len;
      payload = ble_frame_payload_buffer;
      payload_len = ble_frame_payload_buffer_offset;
    }
    // process command
    rpc_request_t *req = (rpc_request_t *) payload;
    esp_err_t ret = rpc_req_handler(req, payload_len);
    // clear buffer
    if (ble_frame_payload_buffer_offset) {
      memset(ble_frame_payload_buffer, 0, ble_frame_payload_buffer_offset);
    }
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "failed to process request");
      return ret;
    }
  }
  return ESP_OK;
}

/* 通过低功耗蓝牙启动安全 protocomm 实例的示例函数 */
protocomm_t *start_pc() {
  protocomm_t *pc = protocomm_new();

  /* 端点 UUID */
  protocomm_ble_name_uuid_t nu_lookup_table[] = {
      {"security_endpoint", 0xFF51},
      {"api_endpoint", 0xFF52}
  };

  /* 配置 protocomm_ble_start() */
  protocomm_ble_config_t config = {
      .service_uuid = {
          /* 最低有效位 <---------------------------------------
          * ---------------------------------------> 最高有效位 */
          0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
          0x00, 0x10, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
      },
      .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
      .nu_lookup = nu_lookup_table
  };

  /* 启动基于低功耗蓝牙的 protocomm 层 */
  protocomm_ble_start(pc, &config);

  /* protocomm_security0 方案不使用所有权证明，因此可以将其保持为 NULL */
  protocomm_set_security(pc, "security_endpoint", &protocomm_security0, NULL);
  protocomm_add_endpoint(pc, "api_endpoint", req_handler, NULL);
  return pc;
}

/* 停止 protocomm 实例的示例函数 */
void stop_pc(protocomm_t *pc) {
  protocomm_remove_endpoint(pc, "api_endpoint");
  protocomm_unset_security(pc, "security_endpoint");

  /* 停止低功耗蓝牙 protocomm 服务 */
  protocomm_ble_stop(pc);

  protocomm_delete(pc);
}