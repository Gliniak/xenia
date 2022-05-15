/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_USER_PROFILE_H_
#define XENIA_KERNEL_XAM_USER_PROFILE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <numeric>

#include "xenia/base/byte_stream.h"
#include "xenia/xbox.h"
#include "xenia/kernel/xam/xdbf/xdbf.h"

namespace xe {
namespace kernel {
namespace xam {

constexpr uint32_t kDashboardID = 0xFFFE07D1;
const static std::string kDashboardStringID =
    fmt::format("{:08X}", kDashboardID);

// https://github.com/jogolden/testdev/blob/master/xkelib/xam/_xamext.h#L68
enum class XTileType {
  kAchievement,
  kGameIcon,
  kGamerTile,
  kGamerTileSmall,
  kLocalGamerTile,
  kLocalGamerTileSmall,
  kBkgnd,
  kAwardedGamerTile,
  kAwardedGamerTileSmall,
  kGamerTileByImageId,
  kPersonalGamerTile,
  kPersonalGamerTileSmall,
  kGamerTileByKey,
  kAvatarGamerTile,
  kAvatarGamerTileSmall,
  kAvatarFullBody
};

// TODO: find filenames of other tile types that are stored in profile
static const std::map<XTileType, std::string> kTileFileNames = {
    {XTileType::kPersonalGamerTile, "tile_64.png"},
    {XTileType::kPersonalGamerTileSmall, "tile_32.png"},
    {XTileType::kAvatarGamerTile, "avtr_64.png"},
    {XTileType::kAvatarGamerTileSmall, "avtr_32.png"},
};

// from https://github.com/xemio/testdev/blob/master/xkelib/xam/_xamext.h
#pragma pack(push, 4)
struct X_XAMACCOUNTINFO {
  enum AccountReservedFlags {
    kPasswordProtected = 0x10000000,
    kLiveEnabled = 0x20000000,
    kRecovering = 0x40000000,
    kVersionMask = 0x000000FF
  };

  enum AccountUserFlags {
    kPaymentInstrumentCreditCard = 1,

    kCountryMask = 0xFF00,
    kSubscriptionTierMask = 0xF00000,
    kLanguageMask = 0x3E000000,

    kParentalControlEnabled = 0x1000000,
  };

  enum AccountSubscriptionTier {
    kSubscriptionTierSilver = 3,
    kSubscriptionTierGold = 6,
    kSubscriptionTierFamilyGold = 9
  };

  enum AccountLiveFlags { kAcctRequiresManagement = 1 };

  xe::be<uint32_t> reserved_flags;
  xe::be<uint32_t> live_flags;
  char16_t gamertag[0x10];
  xe::be<uint64_t> xuid_online;  // 09....
  xe::be<uint32_t> cached_user_flags;
  xe::be<uint32_t> network_id;
  char passcode[4];
  char online_domain[0x14];
  char online_kerberos_realm[0x18];
  char online_key[0x10];
  char passport_membername[0x72];
  char passport_password[0x20];
  char owner_passport_membername[0x72];

  bool IsPasscodeEnabled() {
    return (bool)(reserved_flags & AccountReservedFlags::kPasswordProtected);
  }

  bool IsLiveEnabled() {
    return (bool)(reserved_flags & AccountReservedFlags::kLiveEnabled);
  }

  bool IsRecovering() {
    return (bool)(reserved_flags & AccountReservedFlags::kRecovering);
  }

  bool IsPaymentInstrumentCreditCard() {
    return (bool)(cached_user_flags &
                  AccountUserFlags::kPaymentInstrumentCreditCard);
  }

  bool IsParentalControlled() {
    return (bool)(cached_user_flags &
                  AccountUserFlags::kParentalControlEnabled);
  }

  bool IsXUIDOffline() { return ((xuid_online >> 60) & 0xF) == 0xE; }
  bool IsXUIDOnline() { return ((xuid_online >> 48) & 0xFFFF) == 0x9; }
  bool IsXUIDValid() { return IsXUIDOffline() != IsXUIDOnline(); }
  bool IsTeamXUID() {
    return (xuid_online & 0xFF00000000000140) == 0xFE00000000000100;
  }

  uint32_t GetCountry() { return (cached_user_flags & kCountryMask) >> 8; }

  AccountSubscriptionTier GetSubscriptionTier() {
    return (
        AccountSubscriptionTier)((cached_user_flags & kSubscriptionTierMask) >>
                                 20);
  }

  XLanguage GetLanguage() {
    return (XLanguage)((cached_user_flags & kLanguageMask) >> 25);
  }

  std::string GetGamertagString() const;
};
static_assert_size(X_XAMACCOUNTINFO, 0x17C);
#pragma pack(pop)

struct X_USER_PROFILE_SETTING_DATA {
  // UserProfile::Setting::Type. Appears to be 8-in-32 field, and the upper 24
  // are not always zeroed by the game.
  uint8_t type;
  uint8_t unk_1[3];
  xe::be<uint32_t> unk_4;
  // TODO(sabretooth): not sure if this is a union, but it seems likely.
  // Haven't run into cases other than "binary data" yet.
  union {
    xe::be<int32_t> s32;
    xe::be<int64_t> s64;
    xe::be<uint32_t> u32;
    xe::be<double> f64;
    struct {
      xe::be<uint32_t> size;
      xe::be<uint32_t> ptr;
    } unicode;
    xe::be<float> f32;
    struct {
      xe::be<uint32_t> size;
      xe::be<uint32_t> ptr;
    } binary;
    xe::be<uint64_t> filetime;
  };
};
static_assert_size(X_USER_PROFILE_SETTING_DATA, 16);

struct X_USER_PROFILE_SETTING {
  xe::be<uint32_t> from;
  //xe::be<uint32_t> unk04;
  union {
    xe::be<uint32_t> user_index;
    xe::be<uint64_t> xuid;
  };
  xdbf::X_XDBF_GPD_SETTING setting;
};
static_assert_size(X_USER_PROFILE_SETTING, 40);



class UserProfile {
 public:
  UserProfile(const uint64_t xuid, X_XAMACCOUNTINFO* account_info,
              xam::xdbf::GpdFile dash_gpd,
              std::unordered_map<uint32_t, xdbf::GpdFile> titles_gpd);

  UserProfile(const uint64_t xuid, const X_XAMACCOUNTINFO* account_info);

  X_XAMACCOUNTINFO account() const { return account_; }
  uint64_t xuid() const { return offline_xuid; }
  uint64_t xuid_online() const { return account_.xuid_online; }
  std::string name() const { return account_.GetGamertagString(); }
  uint32_t signin_state() const { return 1; }
  uint32_t type() const { return 1 | 2; /* local | online profile? */ }

  uint32_t GetAmountOfPlayedTitles() const {
    return (uint32_t)title_gpds_.size();
  }

  xdbf::GpdFile* SetTitleSpaData(const xdbf::SpaFile& spa_data);
  xdbf::GpdFile* GetTitleGpd(const uint32_t title_id = -1);
  xdbf::GpdFile* GetDashboardGpd() { return &dash_gpd_; }

  void GetTitles(std::vector<xdbf::GpdFile*>& titles);
  bool UpdateTitleGpd(const uint32_t title_id = -1);
  bool UpdateAllGpds();
  void LoadProfile(const uint64_t xuid);
 private:
  void LoadDefaultSettings();

  bool UpdateGpd(uint32_t title_id, xdbf::GpdFile& gpd_data);
  bool AddSettingIfNotExist(xdbf::Setting& setting);

  xdbf::kAchievementsStats RecalculateAchievementsStats(const uint32_t title_id,
                                                  xdbf::GpdFile& gpd_data);

  X_XAMACCOUNTINFO account_;
  uint64_t offline_xuid;

  std::unordered_map<uint32_t, xdbf::GpdFile> title_gpds_;
  xdbf::GpdFile dash_gpd_;
  xdbf::GpdFile* curr_gpd_ = nullptr;
  uint32_t curr_title_id_ = -1;
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_USER_PROFILE_H_
