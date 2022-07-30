/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_MICROPHONE_DRIVER_H_
#define XENIA_HID_MICROPHONE_DRIVER_H_

#include "xenia/xbox.h"

namespace xe {
namespace hid {

class MicrophoneSystem;

class MicrophoneDriver {
 public:
  virtual ~MicrophoneDriver() = default;

  virtual X_STATUS Setup() = 0;

  virtual X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                                   void* out_caps) = 0;
  virtual X_RESULT GetState(uint32_t user_index, uint32_t* out_state) = 0;
  virtual X_RESULT GetData(uint32_t user_index, void* out_ptr) = 0;

  void set_is_active_callback(std::function<bool()> is_active_callback) {
    is_active_callback_ = is_active_callback;
  }
  
 protected:
  explicit MicrophoneDriver() {}

  bool is_active() const {
    return !is_active_callback_ || is_active_callback_();
  }

private:
  std::function<bool()> is_active_callback_ = nullptr;
};
}
}  // namespace xe

#endif  // XENIA_HID_MICROPHONE_DRIVER_H_