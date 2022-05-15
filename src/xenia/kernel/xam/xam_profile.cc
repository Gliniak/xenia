/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <cstring>

#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xam/user_profile.h"
#include "xenia/kernel/xam/xam_private.h"
#include "xenia/kernel/xenumerator.h"
#include "xenia/kernel/xthread.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
namespace xam {

dword_result_t XamProfileCreate_entry(dword_t flags, lpdword_t device_id,
                                      qword_t xuid,
                                      pointer_t<X_XAMACCOUNTINFO> account,
                                      lpdword_t r7, lpdword_t r8, lpdword_t r9,
                                      lpdword_t r10) {
  if (device_id) {
    *device_id = 0x1;
  }

  if (xuid != 0) {
    // Why is this param even included?
    return X_E_INVALIDARG;
  }

  X_XAMACCOUNTINFO account_info_data;
  memcpy(&account_info_data, account, sizeof(X_XAMACCOUNTINFO));
  xe::copy_and_swap<char16_t>(account_info_data.gamertag,
                              account_info_data.gamertag, 16);

  bool result =
      kernel_state()->profile_manager()->CreateProfile(&account_info_data);

  return result ? X_ERROR_INVALID_PARAMETER : X_STATUS_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamProfileCreate, kUserProfiles, kStub);

dword_result_t XamProfileOpen_entry(qword_t xuid, lpstring_t mount_name) {
  std::string guest_name = mount_name;
  bool result =
      kernel_state()->profile_manager()->MountProfile(xuid, guest_name);

  if (!result) {
    return X_ERROR_FUNCTION_FAILED;
  }
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamProfileOpen, kUserProfiles, kStub);

dword_result_t XamProfileClose_entry(lpstring_t mount_name) {
  std::string guest_name = mount_name;
  bool result =
      kernel_state()->file_system()->UnregisterDevice(guest_name + ':');

  if (!result) {
    return X_ERROR_FUNCTION_FAILED;
  }
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamProfileClose, kUserProfiles, kStub);

struct X_PROFILEENUMRESULT {
  xe::be<uint64_t> xuid_offline;  // E0.....
  X_XAMACCOUNTINFO account;
  xe::be<uint32_t> device_id;
};
static_assert_size(X_PROFILEENUMRESULT, 0x188);

dword_result_t XamProfileCreateEnumerator_entry(dword_t device_id,
                                                lpdword_t handle_out) {
  assert_not_null(handle_out);

  auto e = new XStaticUntypedEnumerator(kernel_state(), 1,
                                        sizeof(X_PROFILEENUMRESULT));

  e->Initialize(0xFF, 0xFF, 0x23001, 0x23003, 0x28);

  const auto& profiles = kernel_state()->profile_manager()->GetProfiles();

  for (const auto profile : profiles) {
    X_PROFILEENUMRESULT* profile_enum = (X_PROFILEENUMRESULT*)e->AppendItem();
    memcpy(&profile_enum->account, &profile.second->account(),
           sizeof(X_XAMACCOUNTINFO));
    profile_enum->xuid_offline = profile.second->xuid();
    profile_enum->device_id = 0x00000001;
    profile_enum->account.xuid_online = profile.second->xuid_online();

    xe::copy_and_swap(profile_enum->account.gamertag,
                      profile.second->account().gamertag, 16);
  }
  *handle_out = e->handle();
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamProfileCreateEnumerator, kUserProfiles, kImplemented);

dword_result_t XamProfileLoadAccountInfo_entry(
    dword_t unk3, qword_t xuid,
    pointer_t<X_XAMACCOUNTINFO> xam_account_info_ptr) {
  if (!xam_account_info_ptr || !xuid) {
    return X_E_INVALIDARG;
  }

  const auto& user_profile =
      kernel_state()->profile_manager()->GetProfileWithXuid(xuid, true);

  if (!user_profile) {
    return X_E_INVALIDARG;
  }

  memcpy(xam_account_info_ptr, &user_profile->account(),
         sizeof(X_XAMACCOUNTINFO));

  xe::copy_and_swap(xam_account_info_ptr->gamertag,
                    user_profile->account().gamertag, 16);

  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamProfileLoadAccountInfo, kUserProfiles, kImplemented);

}  // namespace xam
}  // namespace kernel
}  // namespace xe

DECLARE_XAM_EMPTY_REGISTER_EXPORTS(Profile);