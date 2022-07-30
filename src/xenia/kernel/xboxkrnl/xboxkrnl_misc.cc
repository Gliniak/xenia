/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/logging.h"
#include "xenia/hid/microphone/microphone.h"
#include "xenia/hid/microphone/microphone_system.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xboxkrnl/xboxkrnl_private.h"
#include "xenia/kernel/xthread.h"
#include "xenia/xbox.h"
#include "xenia/emulator.h"

namespace xe {
namespace kernel {
namespace xboxkrnl {

void KeEnableFpuExceptions_entry(dword_t enabled) {
  // TODO(benvanik): can we do anything about exceptions?
}
DECLARE_XBOXKRNL_EXPORT1(KeEnableFpuExceptions, kNone, kStub);

dword_result_t MicDeviceRequest_entry(pointer_t<hid::X_MIC_DEVICE> device_ptr) {
  // Device_ptr + 4 is status
  // Status list:
  // < 1 or 6 - Not Connected
  // 1 to 3 - Connected (Not initialized)
  // 5 - Initialization
  // Any other value (including 4) - Error

  if (!device_ptr) {
    return X_STATUS_INVALID_PARAMETER;
  }
  XELOGE("MicDeviceRequest State: {:08X} Action: {:08X} USER: {:08X}",
         device_ptr->info.state, device_ptr->info.request_type,
         device_ptr->info.user_index);
  // Some of fields are already filled correctly.
  // One example is user_index, because there would be no other way to determine
  // for which user certain microphone is assigned That means that every user
  // must have their one personal microphone class device should at start have
  // (somewhere user index) in form of user_index | mask (0x04000000)
  if (device_ptr->info.user_index > 4) {
    return X_STATUS_INVALID_PARAMETER;
  }

  if (device_ptr->info.request_type > 0xB) {
    return X_STATUS_INVALID_DEVICE_REQUEST;
  }

  auto microphone_system = kernel_state()->emulator()->microphone_system();

  if (!device_ptr->info.request_type) {
    uint32_t new_state = device_ptr->info.state;
    X_RESULT status =
        microphone_system->GetState(device_ptr->info.user_index, &new_state);

    if (status) {
      device_ptr->info.state = 0;
      return X_STATUS_INVALID_DEVICE_REQUEST;
    }

    XELOGE("MicDeviceRequest New State: {:08X}", new_state);
    device_ptr->info.state = new_state;
  }

  switch (device_ptr->info.request_type) {
    case 1:
      // GAIN
      break;
    case 7:
      // Getting data. IO_PENDING
      return X_STATUS_PENDING;
    case 9:
      device_ptr->capabilities.features = 0x100;
      device_ptr->capabilities.format_tag = 1;
      device_ptr->capabilities.mic_color = 0;
      break;
    default:
      // 0 Seems like initialization!
      break;
  }
  return X_ERROR_SUCCESS;
  // return X_STATUS_PENDING;
}
DECLARE_XBOXKRNL_EXPORT1(MicDeviceRequest, kNone, kStub);

}  // namespace xboxkrnl
}  // namespace kernel
}  // namespace xe

DECLARE_XBOXKRNL_EMPTY_REGISTER_EXPORTS(Misc);
