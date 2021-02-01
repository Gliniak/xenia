/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xam/xam_private.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
namespace xam {

dword_result_t XamPartyGetUserList(dword_t player_count, lpdword_t party_list) {
  // Sonic & All-Stars Racing Transformed want specificly this code
  // to skip loading party data.
  // This code is not documented in NT_STATUS code list
  return 0x807D0003;
}
DECLARE_XAM_EXPORT1(XamPartyGetUserList, kNone, kStub);

dword_result_t XamPartySendGameInvites() { return X_ERROR_FUNCTION_FAILED; }
DECLARE_XAM_EXPORT1(XamPartySendGameInvites, kNone, kStub);

dword_result_t XamPartySetCustomData() { return X_ERROR_FUNCTION_FAILED; }
DECLARE_XAM_EXPORT1(XamPartySetCustomData, kNone, kStub);

dword_result_t XamPartyGetBandwidth() { return X_ERROR_FUNCTION_FAILED; }
DECLARE_XAM_EXPORT1(XamPartyGetBandwidth, kNone, kStub);

void RegisterPartyExports(xe::cpu::ExportResolver* export_resolver,
                          KernelState* kernel_state) {}
}  // namespace xam
}  // namespace kernel
}  // namespace xe
