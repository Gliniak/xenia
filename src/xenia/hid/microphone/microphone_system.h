/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_MICROPHONE_SYSTEM_H_
#define XENIA_HID_MICROPHONE_SYSTEM_H_

#include "xenia/xbox.h"

#include "xenia/hid/microphone/microphone.h"
#include "xenia/hid/microphone/microphone_driver.h"

namespace xe {
namespace hid {

class MicrophoneSystem {
 public:
  explicit MicrophoneSystem();
  ~MicrophoneSystem();

  X_STATUS Setup();

  void AddDriver(std::unique_ptr<MicrophoneDriver> driver);
  X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                           XMICCAPABILITIES* out_caps);
  X_RESULT GetState(uint32_t user_index, uint32_t* out_state);
  X_RESULT GetData(uint32_t user_index, void* out_ptr);

 private:
  std::vector<std::unique_ptr<MicrophoneDriver>> drivers_;
};

}  // namespace hid
}  // namespace xe

#endif  // XENIA_HID_MICROPHONE_SYSTEM_H_