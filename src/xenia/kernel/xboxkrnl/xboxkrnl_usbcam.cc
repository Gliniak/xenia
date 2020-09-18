/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xboxkrnl/xboxkrnl_private.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
namespace xboxkrnl {

dword_result_t XUsbcamCreate(dword_t buffer,
                             dword_t buffer_size,  // 0x4B000 640x480?
                             lpdword_t handle_out) {
  // This function should return success.
  // It looks like it only allocates space for usbcam support.
  // returning error code might cause games to initialize incorrectly.
  // "Carcassonne" initalization function checks for result from this
  // function. If value is different than 0 instead of loading
  // rest of the game it returns from initalization function and tries
  // to run game normally which causes crash, due to uninitialized data.
  kernel_memory()->Fill(buffer, buffer_size, 0);

  *handle_out = 0xDEADF00D;
  return X_STATUS_SUCCESS;
}
DECLARE_XBOXKRNL_EXPORT1(XUsbcamCreate, kNone, kStub);

dword_result_t XUsbcamGetState() {
  // 0 - Not connected.
  // 1 - Plugged in
  // 2 - Initialized
  return 0;
}
DECLARE_XBOXKRNL_EXPORT1(XUsbcamGetState, kNone, kStub);

dword_result_t XUsbcamReadFrame(dword_t handle, dword_t buffer, dword_t buffer_size,
                                dword_t r6, dword_t overlapped_ptr) {
  return X_ERROR_IO_PENDING;
}
DECLARE_XBOXKRNL_EXPORT1(XUsbcamReadFrame, kNone, kStub);

dword_result_t XUsbcamSetConfig(dword_t r3, dword_t r4, dword_t r5, dword_t r6,
                                dword_t r7, dword_t r8, dword_t r9) {
  return X_STATUS_SUCCESS;
}
DECLARE_XBOXKRNL_EXPORT1(XUsbcamSetConfig, kNone, kStub);

dword_result_t XUsbcamSetCaptureMode(dword_t handle) {
  return X_STATUS_SUCCESS;
}
DECLARE_XBOXKRNL_EXPORT1(XUsbcamSetCaptureMode, kNone, kStub);

void RegisterUsbcamExports(xe::cpu::ExportResolver* export_resolver,
                           KernelState* kernel_state) {}

}  // namespace xboxkrnl
}  // namespace kernel
}  // namespace xe
