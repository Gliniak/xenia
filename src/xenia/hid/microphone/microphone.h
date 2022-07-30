/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include "xenia/base/assert.h"
#include "xenia/base/byte_order.h"

namespace xe {
namespace hid {

/* States of microphone
   0 - Not connected
   1 - Connected, not initialized for user_index 0
   2 - Connected, not initialized for user_index 1
   3 - Connected, not initialized for user_index 2
   4 - Connected, not initialized for user_index 3
   5 - Ready (for user_index 0?)
*/

typedef struct {
  xe::be<uint16_t> request_type;
  xe::be<uint16_t> user_index;
  xe::be<uint32_t> state;  // 8

  xe::be<uint64_t> unk1;  // 16
  xe::be<uint64_t> unk2;  // 24
} XMICINFO;

typedef struct {
  xe::be<uint32_t> features;
  xe::be<uint16_t> format_tag;
  xe::be<uint16_t> channels;
  xe::be<uint32_t> sample_rates;
  xe::be<uint16_t> bits_per_sample;
  xe::be<uint16_t> frame_length;  // 0xE
  xe::be<uint8_t> mic_color;      // 0x10
  xe::be<uint16_t> vendor_id;
  xe::be<uint16_t> product_id;
  xe::be<uint16_t> revision;
  xe::be<uint32_t> device_id;

} XMICCAPABILITIES;
static_assert_size(XMICCAPABILITIES, 0x1C);

typedef struct {
  XMICINFO info;
  XMICCAPABILITIES capabilities;

} X_MIC_DEVICE;

}  // namespace hid
}  // namespace xe