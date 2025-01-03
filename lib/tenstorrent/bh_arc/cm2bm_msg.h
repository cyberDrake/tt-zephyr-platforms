/*
 * Copyright (c) 2024 Tenstorrent AI ULC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CM2BM_MSG_H
#define CM2BM_MSG_H

#include <stdint.h>

typedef enum {
  kCm2BmMsgIdNull = 0,
  kCm2BmMsgIdResetReq = 1,
} Cm2BmMsgId;

typedef struct {
  uint8_t msg_id;
  uint32_t data;
} Cm2BmMsg;

typedef struct {
  uint8_t msg_id;
  uint8_t seq_num;
  uint32_t data;
} __attribute__((packed)) Cm2BmSmbusReqMsg;

typedef struct {
  uint8_t msg_id;
  uint8_t seq_num;
} __attribute__((packed)) Cm2BmSmbusAckMsg;

int32_t EnqueueCm2BmMsg(const Cm2BmMsg *msg);
int32_t Cm2BmMsgReqSmbusHandler(uint8_t *data, uint8_t size);
int32_t Cm2BmMsgAckSmbusHandler(const uint8_t *data, uint8_t size);
int32_t ResetBoardByte(uint8_t *data, uint8_t size);

void ChipResetRequest(void *arg);

typedef struct bmStaticInfo {
  // Non-zero for valid data
  // Allows for breaking changes
  uint32_t version;
  uint32_t bl_version;
  uint32_t app_version;
} __attribute__((packed)) bmStaticInfo;

int32_t Bm2CmSendDataHandler(const uint8_t *data, uint8_t size);

#endif
