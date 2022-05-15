/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_PROFILE_MANAGER_H_
#define XENIA_KERNEL_XAM_PROFILE_MANAGER_H_

#include <random>
#include <string>
#include <vector>

#include "xenia/kernel/xam/user_profile.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
class KernelState;
}  // namespace kernel
}  // namespace xe

namespace xe {
namespace kernel {
namespace xam {

class ProfileManager {
 public:
  static bool DecryptAccountFile(const uint8_t* data, X_XAMACCOUNTINFO* output,
                                 bool devkit = false);

  static void EncryptAccountFile(const X_XAMACCOUNTINFO* input, uint8_t* output,
                                 bool devkit = false);

  // Profile:
  //  - Account
  //  - GPDs (Dashboard, titles)

  // Loading Profile means load everything
  // Loading Account means load basic data

  ProfileManager(KernelState* kernel_state,
                 const std::filesystem::path& profiles_path);

  ~ProfileManager();

  UserProfile* GetCurrentlyLoggedProfile() { return logged_profiles_[0]; }
  UserProfile* GetProfileWithUserIndex(const uint8_t user_index);
  UserProfile* GetProfileWithXuid(const uint64_t xuid,
                                  bool include_not_signed = false);
  uint8_t GetUserIndexAssignedToProfile(const uint64_t xuid);

  std::map<std::string, UserProfile*> GetProfiles() const { return profiles_; }
  uint32_t GetProfilesCount() const { return (uint32_t)profiles_.size(); }

  bool CreateAccount(const std::filesystem::path profile_path,
                     const uint64_t xuid, const X_XAMACCOUNTINFO* account);
  bool LoadAccount(const std::filesystem::path profile_path);
  void LoadAccounts(const std::vector<std::string> profiles_paths);

  bool DismountProfile(const std::string xuid);
  bool MountProfile(const std::string xuid, const std::string mount_path = "",
                    bool read_only = false);
  bool MountProfile(const uint64_t xuid, const std::string mount_path = "");
  void Login(uint64_t xuid = -1);
  void Logout(const uint64_t xuid);

  bool CreateProfile(const X_XAMACCOUNTINFO* account);

  bool IsAnyProfileSignedIn() const { return !logged_profiles_.empty(); }

 private:
  std::filesystem::path GetProfilePath(const uint64_t xuid);
  std::filesystem::path GetProfilePath(const std::string xuid) {
    return profiles_path_ / xuid / kDashboardStringID /
           fmt::format("{:08X}", XContentType::kProfile) / xuid;
  }
  xam::xdbf::GpdFile ReadProfileDashboardGpd(const std::string xuid);
  std::unordered_map<uint32_t, xdbf::GpdFile> ReadProfileTitlesGpd(
      const std::string xuid, const xam::xdbf::GpdFile dash_gpd);

  bool ReadGPD(const std::string xuid, const uint32_t title_id,
               xam::xdbf::GpdFile& gpd);
  std::vector<std::string> FindProfiles();

  uint8_t FindFirstFreeProfileSlot() const;
  uint8_t GetUsedUserSlots() const;

  uint64_t GenerateXuid() const {
    std::random_device rd;
    std::mt19937 gen(rd());

    return ((uint64_t)0xE03 << 52) + (gen() % (1 << 31));
  }
  // Not sure how it works on console, but to read from profile stfs we're
  // mounting it to some guest path (symlinked)
  std::filesystem::path profiles_path_;

  std::map<std::string, UserProfile*> profiles_;
  std::map<uint8_t, UserProfile*> logged_profiles_;

  KernelState* kernel_state_;
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_PROFILE_MANAGER_H_
