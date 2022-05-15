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
X_HRESULT_result_t XamUserGetXUID_entry(dword_t user_index, dword_t type_mask,
                                        lpqword_t xuid_ptr) {
  assert_true(type_mask == 1 || type_mask == 2 || type_mask == 3 ||
              type_mask == 4 || type_mask == 7);
  if (!xuid_ptr) {
    return X_E_INVALIDARG;
  }

  if (user_index > 3) {
    return X_E_INVALIDARG;
  }
  uint32_t result = X_E_NO_SUCH_USER;
  uint64_t xuid = 0;

  const auto& user_profile = kernel_state()->profile_manager()->GetProfileWithUserIndex(user_index);
  if (!user_profile) {
    return result;
  }

  auto type = user_profile->type() & type_mask;
  if (type & (2 | 4)) {
    // maybe online profile?
    xuid = user_profile->xuid_online();
    result = X_E_SUCCESS;
  } else if (type & 1) {
    // maybe offline profile?
    xuid = user_profile->xuid();
    result = X_E_SUCCESS;
  } else {
    result = X_E_INVALIDARG;
  }
  *xuid_ptr = xuid;
  return result;
}
DECLARE_XAM_EXPORT1(XamUserGetXUID, kUserProfiles, kImplemented);

dword_result_t XamUserGetSigninState_entry(dword_t user_index) {
  // Yield, as some games spam this.
  xe::threading::MaybeYield();
  uint32_t signin_state = 0;
  if (user_index > 3) {
    return 0;
  }

  const auto& user_profile = kernel_state()->profile_manager()->GetProfileWithUserIndex(user_index);
  if (!user_profile) {
    return 0;
  }
  return user_profile->signin_state();
}
DECLARE_XAM_EXPORT2(XamUserGetSigninState, kUserProfiles, kImplemented,
                    kHighFrequency);

typedef struct {
  xe::be<uint64_t> xuid;
  xe::be<uint32_t> unk08;  // maybe zero?
  xe::be<uint32_t> signin_state;
  xe::be<uint32_t> unk10;  // ?
  xe::be<uint32_t> unk14;  // ?
  char name[16];
} X_USER_SIGNIN_INFO;
static_assert_size(X_USER_SIGNIN_INFO, 40);

X_HRESULT_result_t XamUserGetSigninInfo_entry(
    dword_t user_index, dword_t flags, pointer_t<X_USER_SIGNIN_INFO> info) {
  if (!info) {
    return X_E_INVALIDARG;
  }
  std::memset(info, 0, sizeof(X_USER_SIGNIN_INFO));

  const auto& user_profile =
      kernel_state()->profile_manager()->GetProfileWithUserIndex(user_index);

  if (!user_profile) {
    return X_E_NO_SUCH_USER;
  }

  info->xuid = user_profile->xuid();
  info->signin_state = user_profile->signin_state();
  xe::string_util::copy_truncating(info->name, user_profile->name(),
                                   xe::countof(info->name));
  return X_E_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserGetSigninInfo, kUserProfiles, kImplemented);

dword_result_t XamUserGetName_entry(dword_t user_index, lpstring_t buffer,
                                    dword_t buffer_len) {
  if (user_index >= 4) {
    return X_E_INVALIDARG;
  }

  const auto& user_profile =
      kernel_state()->profile_manager()->GetProfileWithUserIndex(user_index);

  if (!user_profile) {
    return X_E_NO_SUCH_USER;
  }
  const auto& user_name = user_profile->name();
  xe::string_util::copy_truncating(buffer, user_name,
                                   std::min(buffer_len.value(), uint32_t(16)));
  return X_E_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserGetName, kUserProfiles, kImplemented);

dword_result_t XamUserGetGamerTag_entry(dword_t user_index,
                                        lpu16string_out_t buffer,
                                        dword_t buffer_len) {
  if (user_index >= 4) {
    return X_E_INVALIDARG;
  }

  if (!buffer || buffer_len < 16) {
    return X_E_INVALIDARG;
  }

  const auto& user_profile =
      kernel_state()->profile_manager()->GetProfileWithUserIndex(user_index);

  if (!user_profile) {
    return X_E_NO_SUCH_USER;
  }
  auto user_name = xe::to_utf16(user_profile->name());
  xe::string_util::copy_and_swap_truncating(
      buffer, user_name, std::min(buffer_len.value(), uint32_t(16)));
  return X_E_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserGetGamerTag, kUserProfiles, kImplemented);

typedef struct {
  xe::be<uint32_t> setting_count;
  xe::be<uint32_t> settings_ptr;
} X_USER_READ_PROFILE_SETTINGS;
static_assert_size(X_USER_READ_PROFILE_SETTINGS, 8);

typedef struct {
  xe::be<uint32_t> from;
  xe::be<uint32_t> unk04;
  union {
    xe::be<uint32_t> user_index;
    xe::be<uint64_t> user_xuid;
  } user;
  xdbf::X_XDBF_GPD_SETTING setting;
} X_USER_READ_PROFILE_SETTING;
static_assert_size(X_USER_READ_PROFILE_SETTING, 40);

// https://github.com/oukiar/freestyledash/blob/master/Freestyle/Tools/Generic/xboxtools.cpp
uint32_t XamUserReadProfileSettingsEx(uint32_t title_id, uint32_t user_index,
                                      uint32_t xuid_count, be<uint64_t>* xuids,
                                      uint32_t setting_count,
                                      be<uint32_t>* setting_ids, uint32_t unk,
                                      be<uint32_t>* buffer_size_ptr,
                                      uint8_t* buffer,
                                      XAM_OVERLAPPED* overlapped) {
  // must have at least 1 to 32 settings
  if (setting_count < 1 || setting_count > 32) {
    return X_ERROR_INVALID_PARAMETER;
  }

  // buffer size pointer must be valid
  if (!buffer_size_ptr) {
    return X_ERROR_INVALID_PARAMETER;
  }

  // if buffer size is non-zero, buffer pointer must be valid
  auto buffer_size = static_cast<uint32_t>(*buffer_size_ptr);
  if (buffer_size && !buffer) {
    return X_ERROR_INVALID_PARAMETER;
  }

  uint64_t xuid = 0;
  if (xuid_count && xuids) {
    xuid = *xuids;
  }

  // TODO(gibbed): why is this a thing?
  uint32_t actual_user_index = user_index;
  if (actual_user_index == 255) {
    actual_user_index = 0;
  }

  uint32_t actual_title_id = title_id;
  if (!actual_title_id) {
    actual_title_id = kernel_state()->title_id();
  }
  // Title ID = 0 means us.
  // 0xfffe07d1 = profile?
  auto user_profile =
      kernel_state()->profile_manager()->GetProfileWithUserIndex(
          actual_user_index);

  if (xuid) {
    user_profile = kernel_state()->profile_manager()->GetProfileWithXuid(xuid);
  }

  if (!user_profile) {
    // Only support user 0.
    if (overlapped) {
      kernel_state()->CompleteOverlappedImmediate(
          kernel_state()->memory()->HostToGuestVirtual(overlapped),
          X_ERROR_NOT_FOUND);
      return X_ERROR_IO_PENDING;
    }
    return X_ERROR_NOT_FOUND;
  }

  // First call asks for size (fill buffer_size_ptr).
  // Second call asks for buffer contents with that size.

  // Compute required base size.
  uint32_t base_size_needed = sizeof(X_USER_READ_PROFILE_SETTINGS);
  base_size_needed += setting_count * sizeof(X_USER_READ_PROFILE_SETTING);

  // Compute required extra size.
  uint32_t size_needed = base_size_needed;
  for (uint32_t n = 0; n < setting_count; ++n) {
    auto setting_id = (xdbf::X_XDBF_SETTING_ID)(uint32_t)setting_ids[n];
    xdbf::Setting setting;
    if (user_profile->GetDashboardGpd()->GetSetting(setting_id, &setting)) {
      size_needed += (uint32_t)setting.extraData.size();
    } else {
      XELOGE(
          "XamUserReadProfileSettings requested unimplemented setting {:08X}",
          setting_id);
    }
  }

  *buffer_size_ptr = size_needed;
  if (!buffer || buffer_size < size_needed) {
    if (overlapped) {
      kernel_state()->CompleteOverlappedImmediate(
          kernel_state()->memory()->HostToGuestVirtual(overlapped),
          X_ERROR_INSUFFICIENT_BUFFER);
      return X_ERROR_IO_PENDING;
    }
    return X_ERROR_INSUFFICIENT_BUFFER;
  }

  auto out_header = reinterpret_cast<X_USER_READ_PROFILE_SETTINGS*>(buffer);
  out_header->setting_count = static_cast<uint32_t>(setting_count);
  out_header->settings_ptr =
      kernel_state()->memory()->HostToGuestVirtual(buffer) + 8;

  auto out_setting = reinterpret_cast<X_USER_READ_PROFILE_SETTING*>(buffer + 8);

  auto gpd = user_profile->GetDashboardGpd();
  if (title_id != 0xFFFE07D1) {
    gpd = user_profile->GetTitleGpd(title_id);
  }

  uint32_t buffer_offset = base_size_needed;
  for (uint32_t n = 0; n < setting_count; ++n) {
    auto setting_id = (xdbf::X_XDBF_SETTING_ID)(uint32_t)setting_ids[n];
    xdbf::Setting setting;
    bool exists = gpd && gpd->GetSetting(setting_id, &setting);

    if (!exists) {
      XELOGE("Setting {:08X} Doesn't exist! Creating entry in GPD", setting_id);

      //xdbf::Setting setting(setting_id, (uint32_t)0);
      //gpd->UpdateSetting(setting);

      //exists = gpd && gpd->GetSetting(setting_id, &setting);
    }
    // TODO: fix binary & unicode settings crashing dash.xex!
    if (setting.id == xdbf::XPROFILE_GAMERCARD_AVATAR_INFO_1) {
      exists = false;
    }

    std::memset(out_setting, 0, sizeof(X_USER_READ_PROFILE_SETTING));
    out_setting->from = !exists ? 0 : setting.IsTitleSpecific() ? 2 : 1;
    out_setting->setting.setting_id = setting_id;

    if (xuid_count && xuids) {
      out_setting->user.user_xuid = xuid;
    } else {
      out_setting->user.user_xuid = -1;
      out_setting->user.user_index = actual_user_index;
    }

    if (exists) {
      memcpy(&out_setting->setting.value, &setting.value,
             sizeof(xdbf::X_XUSER_DATA));

      if (setting.value.type == xdbf::X_XUSER_DATA_TYPE::kBinary) {
        memcpy(buffer + buffer_offset, setting.extraData.data(),
               setting.extraData.size());

        out_setting->setting.value.binary.size =
            (uint32_t)setting.extraData.size();
        out_setting->setting.value.binary.ptr =
            kernel_state()->memory()->HostToGuestVirtual(buffer) +
            buffer_offset;

        buffer_offset += (uint32_t)setting.extraData.size();
      } else if (setting.value.type == xdbf::X_XUSER_DATA_TYPE::kUnicode) {
        memcpy(buffer + buffer_offset, setting.extraData.data(),
               setting.extraData.size());

        out_setting->setting.value.unicode.size =
            (uint32_t)setting.extraData.size();
        out_setting->setting.value.unicode.ptr =
            kernel_state()->memory()->HostToGuestVirtual(buffer) +
            buffer_offset;

        buffer_offset += (uint32_t)setting.extraData.size();
      }
    }
    ++out_setting;
  }

  if (overlapped) {
    kernel_state()->CompleteOverlappedImmediate(
        kernel_state()->memory()->HostToGuestVirtual(overlapped),
        X_ERROR_SUCCESS);
    return X_ERROR_IO_PENDING;
  }
  return X_ERROR_SUCCESS;
}

dword_result_t XamUserReadProfileSettings_entry(
    dword_t title_id, dword_t user_index, dword_t xuid_count, lpqword_t xuids,
    dword_t setting_count, lpdword_t setting_ids, lpdword_t buffer_size_ptr,
    lpvoid_t buffer_ptr, pointer_t<XAM_OVERLAPPED> overlapped) {
  return XamUserReadProfileSettingsEx(title_id, user_index, xuid_count, xuids,
                                      setting_count, setting_ids, 0,
                                      buffer_size_ptr, buffer_ptr, overlapped);
}
DECLARE_XAM_EXPORT1(XamUserReadProfileSettings, kUserProfiles, kImplemented);

dword_result_t XamUserReadProfileSettingsEx_entry(
    dword_t title_id, dword_t user_index, dword_t xuid_count, lpqword_t xuids,
    dword_t setting_count, lpdword_t setting_ids, lpdword_t buffer_size_ptr,
    dword_t unk_2, lpvoid_t buffer_ptr, pointer_t<XAM_OVERLAPPED> overlapped) {
  return XamUserReadProfileSettingsEx(title_id, user_index, xuid_count, xuids,
                                      setting_count, setting_ids, unk_2,
                                      buffer_size_ptr, buffer_ptr, overlapped);
}
DECLARE_XAM_EXPORT1(XamUserReadProfileSettingsEx, kUserProfiles, kImplemented);

dword_result_t XamUserWriteProfileSettings_entry(
    dword_t title_id, dword_t user_index, dword_t setting_count,
    pointer_t<X_USER_PROFILE_SETTING> settings,
    pointer_t<XAM_OVERLAPPED> overlapped) {
  if (!setting_count || !settings) {
    return X_ERROR_INVALID_PARAMETER;
  }

  if (user_index) {
    // Only support user 0.
    if (overlapped) {
      kernel_state()->CompleteOverlappedImmediate(overlapped,
                                                  X_ERROR_NOT_FOUND);
      return X_ERROR_IO_PENDING;
    }
    return X_ERROR_NO_SUCH_USER;
  }

  // Update and save settings.
  const auto& user_profile =
      kernel_state()->profile_manager()->GetProfileWithUserIndex(user_index);

  auto gpd = user_profile->GetDashboardGpd();
  if (title_id != kDashboardID) {
    gpd = user_profile->GetTitleGpd(title_id);
  }

  if (!gpd) {
    // TODO: find out proper error code for this condition!
    if (overlapped) {
      kernel_state()->CompleteOverlappedImmediate(overlapped,
                                                  X_ERROR_INVALID_PARAMETER);
      return X_ERROR_IO_PENDING;
    }
    return X_ERROR_INVALID_PARAMETER;
  }

  for (uint32_t n = 0; n < setting_count; ++n) {
    const X_USER_PROFILE_SETTING& settings_data = settings[n];

    /* auto setting_type =
        static_cast<UserProfile::Setting::Type>(setting.data.type);
    if (setting_type == UserProfile::Setting::Type::UNSET) {
      continue;
    }*/

    XELOGD(
        "XamUserWriteProfileSettings: setting index [{}]:"
        " from={} setting_id={:08X} data.type={}",
        n, (uint32_t)settings_data.from,
        (uint32_t)settings_data.setting.setting_id,
        settings_data.setting.value.type);

    xdbf::Setting setting;
    setting.id = settings_data.setting.setting_id;
    setting.value.type = settings_data.setting.value.type;

    // Retrieve any existing setting data if we can
    gpd->GetSetting(setting.id, &setting);

    // ... and then overwrite it
    memcpy(&setting.value, &settings_data.setting.value,
           sizeof(xdbf::X_XUSER_DATA));

    if (settings_data.setting.value.type == xdbf::X_XUSER_DATA_TYPE::kBinary) {
      if (settings_data.setting.value.binary.ptr) {
        setting.extraData.resize(settings_data.setting.value.binary.size);
        auto* data_ptr = kernel_memory()->TranslateVirtual(
            settings_data.setting.value.binary.ptr);
        memcpy(setting.extraData.data(), data_ptr,
               settings_data.setting.value.binary.size);
      }
    } else if (settings_data.setting.value.type ==
               xdbf::X_XUSER_DATA_TYPE::kUnicode) {
      if (settings_data.setting.value.unicode.ptr) {
        setting.extraData.resize(settings_data.setting.value.unicode.size);
        auto* data_ptr = kernel_memory()->TranslateVirtual(
            settings_data.setting.value.unicode.ptr);
        memcpy(setting.extraData.data(), data_ptr,
               settings_data.setting.value.unicode.size);
      }
    }

    gpd->UpdateSetting(setting);
  }

  if (overlapped) {
    kernel_state()->CompleteOverlappedImmediate(overlapped, X_ERROR_SUCCESS);
    return X_ERROR_IO_PENDING;
  }
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserWriteProfileSettings, kUserProfiles, kImplemented);

dword_result_t XamUserCheckPrivilege_entry(dword_t user_index, dword_t mask,
                                           lpdword_t out_value) {
  // checking all users?
  if (user_index != 0xFF) {
    if (user_index >= 4) {
      return X_ERROR_INVALID_PARAMETER;
    }

    if (user_index) {
      return X_ERROR_NO_SUCH_USER;
    }
  }

  // If we deny everything, games should hopefully not try to do stuff.
  *out_value = 0;
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserCheckPrivilege, kUserProfiles, kStub);

dword_result_t XamUserContentRestrictionGetFlags_entry(dword_t user_index,
                                                       lpdword_t out_flags) {
  if (user_index) {
    return X_ERROR_NO_SUCH_USER;
  }

  // No restrictions?
  *out_flags = 0;
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserContentRestrictionGetFlags, kUserProfiles, kStub);

dword_result_t XamUserContentRestrictionGetRating_entry(dword_t user_index,
                                                        dword_t unk1,
                                                        lpdword_t out_unk2,
                                                        lpdword_t out_unk3) {
  if (user_index) {
    return X_ERROR_NO_SUCH_USER;
  }

  // Some games have special case paths for 3F that differ from the failure
  // path, so my guess is that's 'don't care'.
  *out_unk2 = 0x3F;
  *out_unk3 = 0;
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserContentRestrictionGetRating, kUserProfiles, kStub);

dword_result_t XamUserContentRestrictionCheckAccess_entry(
    dword_t user_index, dword_t unk1, dword_t unk2, dword_t unk3, dword_t unk4,
    lpdword_t out_unk5, dword_t overlapped_ptr) {
  *out_unk5 = 1;

  if (overlapped_ptr) {
    // TODO(benvanik): does this need the access arg on it?
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr,
                                                X_ERROR_SUCCESS);
  }

  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserContentRestrictionCheckAccess, kUserProfiles, kStub);

dword_result_t XamUserIsOnlineEnabled_entry(dword_t user_index) { return 1; }
DECLARE_XAM_EXPORT1(XamUserIsOnlineEnabled, kUserProfiles, kStub);

dword_result_t XamUserGetMembershipTier_entry(dword_t user_index) {
  if (user_index >= 4) {
    return X_ERROR_INVALID_PARAMETER;
  }
  if (user_index) {
    return X_ERROR_NO_SUCH_USER;
  }
  return 6 /* 6 appears to be Gold */;
}
DECLARE_XAM_EXPORT1(XamUserGetMembershipTier, kUserProfiles, kStub);

dword_result_t XamUserAreUsersFriends_entry(dword_t user_index, dword_t unk1,
                                            dword_t unk2, lpdword_t out_value,
                                            dword_t overlapped_ptr) {
  uint32_t are_friends = 0;
  X_RESULT result;

  if (user_index >= 4) {
    result = X_ERROR_INVALID_PARAMETER;
  } else {
    if (user_index == 0) {
      const auto& user_profile =
          kernel_state()->profile_manager()->GetProfileWithUserIndex(
              user_index);
      if (user_profile->signin_state() == 0) {
        result = X_ERROR_NOT_LOGGED_ON;
      } else {
        // No friends!
        are_friends = 0;
        result = X_ERROR_SUCCESS;
      }
    } else {
      // Only support user 0.
      result =
          X_ERROR_NO_SUCH_USER;  // if user is local -> X_ERROR_NOT_LOGGED_ON
    }
  }

  if (out_value) {
    assert_true(!overlapped_ptr);
    *out_value = result == X_ERROR_SUCCESS ? are_friends : 0;
    return result;
  } else if (overlapped_ptr) {
    assert_true(!out_value);
    kernel_state()->CompleteOverlappedImmediateEx(
        overlapped_ptr,
        result == X_ERROR_SUCCESS ? X_ERROR_SUCCESS : X_ERROR_FUNCTION_FAILED,
        X_HRESULT_FROM_WIN32(result),
        result == X_ERROR_SUCCESS ? are_friends : 0);
    return X_ERROR_IO_PENDING;
  } else {
    assert_always();
    return X_ERROR_INVALID_PARAMETER;
  }
}
DECLARE_XAM_EXPORT1(XamUserAreUsersFriends, kUserProfiles, kStub);

struct X_ACHIEVEMENT_UNLOCK_TIME {
  xe::be<uint32_t> dwHighDateTime;
  xe::be<uint32_t> dwLowDateTime;

  X_ACHIEVEMENT_UNLOCK_TIME(uint64_t unlock_time) {
    dwHighDateTime = uint32_t(unlock_time >> 32);
    dwLowDateTime = uint32_t(unlock_time);
  }
};

struct X_ACHIEVEMENT_DETAILS {
  xe::be<uint32_t> id;
  xe::be<uint32_t> label_ptr;
  xe::be<uint32_t> description_ptr;
  xe::be<uint32_t> unachieved_ptr;
  xe::be<uint32_t> image_id;
  xe::be<uint32_t> gamerscore;
  X_ACHIEVEMENT_UNLOCK_TIME unlock_time;
  xe::be<uint32_t> flags;

  static const size_t kStringBufferSize = 464;
};
static_assert_size(X_ACHIEVEMENT_DETAILS, 36);

class XStaticAchievementEnumerator : public XEnumerator {
 public:
  struct AchievementDetails {
    uint32_t id;
    std::u16string label;
    std::u16string description;
    std::u16string unachieved;
    uint32_t image_id;
    uint32_t gamerscore;
    uint64_t unlock_time;
    uint32_t flags;
  };

  XStaticAchievementEnumerator(KernelState* kernel_state,
                               size_t items_per_enumerate, uint32_t flags)
      : XEnumerator(
            kernel_state, items_per_enumerate,
            (sizeof(X_ACHIEVEMENT_DETAILS) - 4) +
                (!!(flags & 7) ? X_ACHIEVEMENT_DETAILS::kStringBufferSize : 0)),
        flags_(flags) {}

  void AppendItem(AchievementDetails item) {
    items_.push_back(std::move(item));
  }

  uint32_t WriteItems(uint32_t buffer_ptr, uint8_t* buffer_data,
                      uint32_t* written_count) override {
    size_t count =
        std::min(items_.size() - current_item_, items_per_enumerate());
    if (!count) {
      return X_ERROR_NO_MORE_FILES;
    }

    size_t size = count * item_size();

    auto details = reinterpret_cast<X_ACHIEVEMENT_DETAILS*>(buffer_data);
    size_t string_offset =
        items_per_enumerate() * sizeof(X_ACHIEVEMENT_DETAILS);
    auto string_buffer =
        StringBuffer{buffer_ptr + static_cast<uint32_t>(string_offset),
                     &buffer_data[string_offset],
                     count * X_ACHIEVEMENT_DETAILS::kStringBufferSize};
    for (size_t i = 0, o = current_item_; i < count; ++i, ++current_item_) {
      const auto& item = items_[current_item_];
      details[i].id = item.id;
      details[i].label_ptr =
          !!(flags_ & 1) ? AppendString(string_buffer, item.label) : 0;
      details[i].description_ptr =
          !!(flags_ & 2) ? AppendString(string_buffer, item.description) : 0;
      details[i].unachieved_ptr =
          !!(flags_ & 4) ? AppendString(string_buffer, item.unachieved) : 0;
      details[i].image_id = item.image_id;
      details[i].gamerscore = item.gamerscore;
      details[i].unlock_time = X_ACHIEVEMENT_UNLOCK_TIME(item.unlock_time);
      if (!(item.flags &
            static_cast<uint32_t>(xdbf::AchievementFlags::kAchievedOnline))) {
        details[i].unlock_time = X_ACHIEVEMENT_UNLOCK_TIME(0);
      }
      details[i].flags = item.flags;
    }

    if (written_count) {
      *written_count = static_cast<uint32_t>(count);
    }

    return X_ERROR_SUCCESS;
  }

 private:
  struct StringBuffer {
    uint32_t ptr;
    uint8_t* data;
    size_t remaining_bytes;
  };

  uint32_t AppendString(StringBuffer& sb, const std::u16string_view string) {
    size_t count = string.length() + 1;
    size_t size = count * sizeof(char16_t);
    if (size > sb.remaining_bytes) {
      assert_always();
      return 0;
    }
    auto ptr = sb.ptr;
    string_util::copy_and_swap_truncating(reinterpret_cast<char16_t*>(sb.data),
                                          string, count);
    sb.ptr += static_cast<uint32_t>(size);
    sb.data += size;
    sb.remaining_bytes -= size;
    return ptr;
  }

 private:
  uint32_t flags_;
  std::vector<AchievementDetails> items_;
  size_t current_item_ = 0;
};

dword_result_t XamUserCreateAchievementEnumerator_entry(
    dword_t title_id, dword_t user_index, dword_t xuid, dword_t flags,
    dword_t offset, dword_t count, lpdword_t buffer_size_ptr,
    lpdword_t handle_ptr) {
  if (!count || !buffer_size_ptr || !handle_ptr) {
    return X_ERROR_INVALID_PARAMETER;
  }

  if (user_index >= 4) {
    return X_ERROR_INVALID_PARAMETER;
  }

  size_t entry_size = sizeof(X_ACHIEVEMENT_DETAILS);
  if (flags & 7) {
    entry_size += X_ACHIEVEMENT_DETAILS::kStringBufferSize;
  }

  auto* game_gpd = kernel_state()
                       ->profile_manager()
                       ->GetProfileWithUserIndex(user_index)
                       ->GetTitleGpd(title_id);
  if (!game_gpd) {
    XELOGE(
        "XamUserCreateAchievementEnumerator called without GPD being loaded!");
    return X_ERROR_SUCCESS;
  }

  std::vector<xam::xdbf::Achievement> achievements;
  game_gpd->GetAchievements(&achievements);

  if (buffer_size_ptr) {
    *buffer_size_ptr = static_cast<uint32_t>(entry_size) * count;
  }

  auto e = object_ref<XStaticAchievementEnumerator>(
      new XStaticAchievementEnumerator(kernel_state(), count, flags));
  auto result = e->Initialize(user_index, 0xFB, 0xB000A, 0xB000B, 0);
  if (XFAILED(result)) {
    return result;
  }

  for (auto ach : achievements) {
    auto item = XStaticAchievementEnumerator::AchievementDetails{
        ach.id,       ach.label,      ach.description, ach.unachieved_desc,
        ach.image_id, ach.gamerscore, ach.unlock_time, ach.flags};
    e->AppendItem(item);
  }
  XELOGD("XamUserCreateAchievementEnumerator: added {} items to enumerator",
         e->items_per_enumerate());

  *handle_ptr = e->handle();
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserCreateAchievementEnumerator, kUserProfiles,
                    kSketchy);

dword_result_t XamUserCreateTitlesPlayedEnumerator_entry(
    dword_t user_index, dword_t xuid, dword_t flags, dword_t offset,
    dword_t games_count, lpdword_t buffer_size_ptr, lpdword_t handle_ptr) {
  std::vector<xdbf::TitlePlayed> titles;
  kernel_state()
      ->profile_manager()
      ->GetProfileWithUserIndex(user_index)
      ->GetDashboardGpd()
      ->GetTitles(&titles);

  // Sort titles by date played
  std::sort(titles.begin(), titles.end(),
            [](const xdbf::TitlePlayed& first, const xdbf::TitlePlayed& second)
                -> bool { return first.last_played > second.last_played; });

  auto e = object_ref<XStaticUntypedEnumerator>(new XStaticUntypedEnumerator(
      kernel_state(), games_count, sizeof(xdbf::X_XDBF_GPD_TITLEPLAYED)));

  auto result = e->Initialize(user_index, 0xFB, 0xB000A, 0xB000B, 0);
  if (XFAILED(result)) {
    return result;
  }

  *handle_ptr = e->handle();

  for (auto title : titles) {
    // For some reason dashboard gpd stores info about itself
    if (title.title_id == kDashboardID) continue;

    auto* details = (xdbf::X_XDBF_GPD_TITLEPLAYED*)e->AppendItem();
    title.WriteGPD(details);
  }

  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserCreateTitlesPlayedEnumerator, kUserProfiles, kStub);

dword_result_t XamSessionCreateHandle_entry(lpdword_t handle_ptr) {
  *handle_ptr = 0xCAFEDEAD;
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamSessionCreateHandle, kUserProfiles, kStub);

dword_result_t XamSessionRefObjByHandle_entry(dword_t handle,
                                              lpdword_t obj_ptr) {
  assert_true(handle == 0xCAFEDEAD);
  // TODO(PermaNull): Implement this properly,
  // For the time being returning 0xDEADF00D will prevent crashing.
  *obj_ptr = 0xDEADF00D;
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamSessionRefObjByHandle, kUserProfiles, kStub);

dword_result_t XamUserGetMembershipTierFromXUID_entry(qword_t xuid) {
  const auto& user =
      kernel_state()->profile_manager()->GetProfileWithXuid(xuid);

  if (!user) {
    return X_ERROR_NO_SUCH_USER;
  }
  return 6;
}
DECLARE_XAM_EXPORT1(XamUserGetMembershipTierFromXUID, kUserProfiles, kStub);

dword_result_t XamUserLogon_entry(lpqword_t xuid, dword_t unk,
                                  dword_t overlapped_ptr) {
  uint64_t profile_xuid = *xuid;

  auto run = [profile_xuid](uint32_t& extended_error,
                            uint32_t& length) -> X_RESULT {
    kernel_state()->profile_manager()->Login(profile_xuid);
    extended_error = 0;
    length = 0;
    return X_ERROR_SUCCESS;
  };

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedDeferredEx(run, overlapped_ptr);
    return X_ERROR_IO_PENDING;
  } else {
    uint32_t extended_error;
    uint32_t item_count;
    X_RESULT result = run(extended_error, item_count);
  }
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserLogon, kUserProfiles, kStub);

dword_result_t XamUserGetIndexFromXUID_entry(qword_t xuid, dword_t unk4,
                                             lpdword_t user_index_ptr) {
  uint8_t assigned_slot = kernel_state()->profile_manager()->GetUserIndexAssignedToProfile(xuid);

  if (assigned_slot == -1) {
    return X_ERROR_NO_SUCH_USER;
  }
  *user_index_ptr = assigned_slot;
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamUserGetIndexFromXUID, kUserProfiles, kStub);



}  // namespace xam
}  // namespace kernel
}  // namespace xe

DECLARE_XAM_EMPTY_REGISTER_EXPORTS(User);
