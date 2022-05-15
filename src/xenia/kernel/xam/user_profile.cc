/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/user_profile.h"

#include <filesystem>
#include <sstream>

#include "third_party/fmt/include/fmt/format.h"
#include "xenia/base/clock.h"
#include "xenia/base/cvar.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/logging.h"
#include "xenia/base/mapped_memory.h"
#include "xenia/emulator.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/crypto_utils.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/vfs/devices/host_path_device.h"
#include "xenia/vfs/devices/stfs_container_device.h"
#include "xenia/vfs/devices/stfs_container_entry.h"
#include "xenia/vfs/devices/stfs_container_file.h"
#include "xenia/kernel/xam/profile_manager.h"

DECLARE_int32(license_mask);



namespace xe {
namespace kernel {
namespace xam {

std::string X_XAMACCOUNTINFO::GetGamertagString() const {
  return xe::to_utf8(std::u16string(gamertag));
}


UserProfile::UserProfile(const uint64_t xuid,
                         const X_XAMACCOUNTINFO* account_info)
    : dash_gpd_(kDashboardID) {
  // 58410A1F checks the user XUID against a mask of 0x00C0000000000000 (3<<54),
  // if non-zero, it prevents the user from playing the game.
  // "You do not have permissions to perform this operation."
  offline_xuid = xuid;
  memcpy(&account_, account_info, sizeof(X_XAMACCOUNTINFO));
}

UserProfile::UserProfile(
    const uint64_t xuid, X_XAMACCOUNTINFO* account_info,
    xam::xdbf::GpdFile dash_gpd,
    std::unordered_map<uint32_t, xdbf::GpdFile> titles_gpd) {

  offline_xuid = xuid;
  memcpy(&account_, account_info, sizeof(X_XAMACCOUNTINFO));
  dash_gpd_ = dash_gpd;
  title_gpds_ = titles_gpd;

  LoadDefaultSettings();
  // Make sure the dash GPD is up-to-date
  //UpdateGpd(kDashboardID, dash_gpd_);
}

void UserProfile::LoadDefaultSettings() {
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMER_YAXIS_INVERSION, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_OPTION_CONTROLLER_VIBRATION, 3u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_ZONE, 0u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_REGION, 0u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_CRED, 0u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_HAS_VISION, 0u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_REP, 0.0f));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_OPTION_VOICE_MUTED, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_OPTION_VOICE_THRU_SPEAKERS, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_OPTION_VOICE_VOLUME, 0x64u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_PICTURE_KEY,
                                     xe::to_utf16("gamercard_picture_key")));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMERCARD_PERSONAL_PICTURE,
                    xe::to_utf16("gamercard_personal_picture")));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_MOTTO,
                                     xe::to_utf16("gamercard_motto")));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMERCARD_TITLES_PLAYED, 1u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED, 0u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMER_DIFFICULTY, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_CONTROL_SENSITIVITY, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_PREFERRED_COLOR_FIRST, 0xFFFF0000u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_PREFERRED_COLOR_SECOND, 0xFF00FF00u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMER_ACTION_AUTO_AIM, 1u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_ACTION_AUTO_CENTER, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_RACE_TRANSMISSION, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_RACE_CAMERA_LOCATION, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_RACE_BRAKE_CONTROL, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMER_RACE_ACCELERATOR_CONTROL, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMERCARD_TITLE_CRED_EARNED, 0u));
  AddSettingIfNotExist(
      xdbf::Setting(xdbf::XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED, 0u));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_USER_NAME,
                                     xe::to_utf16("XeniaUserName")));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_USER_LOCATION,
                                     xe::to_utf16("XeniaUserLocation")));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_USER_URL,
                                     xe::to_utf16("XeniaUserUrl")));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_USER_BIO,
                                     xe::to_utf16("XeniaUserBio")));

  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_TITLE_SPECIFIC1, {}));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_TITLE_SPECIFIC2, {}));
  AddSettingIfNotExist(xdbf::Setting(xdbf::XPROFILE_TITLE_SPECIFIC3, {}));
}


xam::xdbf::GpdFile* UserProfile::SetTitleSpaData(
    const xam::xdbf::SpaFile& spa_data) {
  xdbf::X_XDBF_XTHD_DATA title_data;
  spa_data.GetTitleData(&title_data);

  curr_title_id_ = title_data.title_id;
  curr_gpd_ = nullptr;

  std::vector<xam::xdbf::Achievement> spa_achievements;
  // TODO: let user choose locale?
  spa_data.GetAchievements(spa_data.GetDefaultLocale(), &spa_achievements);

  bool title_included =
      title_data.title_type == xdbf::X_XDBF_XTHD_DATA::TitleType::kFull ||
      title_data.title_type == xdbf::X_XDBF_XTHD_DATA::TitleType::kDownload;

  if (title_data.flags &
      (uint32_t)xdbf::X_XDBF_XTHD_DATA::Flags::kAlwaysIncludeInProfile) {
    title_included = true;
  }

  if (title_data.flags &
      (uint32_t)xdbf::X_XDBF_XTHD_DATA::Flags::kNeverIncludeInProfile) {
    title_included = false;
  }

  // If arcade game, only include if license_mask is set
  if ((title_data.title_id >> 16) == 0x5841) {
    title_included = cvars::license_mask != 0;
  }

  xdbf::TitlePlayed title_info;
  auto gpd = title_gpds_.find(curr_title_id_);

  if (gpd != title_gpds_.end()) {
    auto& title_gpd = (*gpd).second;

    XELOGI("Loaded existing GPD for title {:08X}", curr_title_id_);

    bool always_update_title = false;
    if (!dash_gpd_.GetTitle(curr_title_id_, &title_info)) {
      assert_always();
      XELOGE(
          "GPD exists but is missing XbdfTitlePlayed entry? (this shouldn't be "
          "happening!)");
      // Try to work around it...
      title_info.title_name = xe::to_utf16(spa_data.GetTitleName());
      title_info.title_id = curr_title_id_;
      title_info.achievement_stats.achievements_count = 0;
      title_info.achievement_stats.achievements_earned = 0;
      title_info.achievement_stats.gamerscore_total = 0;
      title_info.achievement_stats.gamerscore_earned = 0;
      always_update_title = true;
    }
    title_info.last_played = Clock::QueryHostSystemTime();

    // Check SPA for any achievements current GPD might be missing
    // (maybe added in TUs etc?)
    bool ach_updated = false;
    for (auto ach : spa_achievements) {
      bool ach_exists = title_gpd.GetAchievement(ach.id, nullptr);
      if (ach_exists && !always_update_title) {
        continue;
      }

      // Achievement doesn't exist in current title info, lets add it
      title_info.achievement_stats.achievements_count++;
      title_info.achievement_stats.gamerscore_total += ach.gamerscore;

      // If it doesn't exist in GPD, add it to that too
      if (!ach_exists) {
        XELOGD(
            "Adding new achievement {} {} from SPA (wasn't inside existing "
            "GPD)",
            ach.id, xe::to_utf8(ach.label));

        ach_updated = true;
        title_gpd.UpdateAchievement(ach);
      }
    }

    // Update dash with new title_info
    dash_gpd_.UpdateTitle(title_info);

    // Only write game GPD if achievements were updated
    if (ach_updated) {
      UpdateGpd(curr_title_id_, title_gpd);
    }
    UpdateGpd(kDashboardID, dash_gpd_);
  } else {
    // GPD not found... have to create it!
    XELOGD("Creating new GPD for title {:08X}", curr_title_id_);

    title_info.title_name = xe::to_utf16(spa_data.GetTitleName());
    title_info.title_id = curr_title_id_;
    title_info.last_played = Clock::QueryHostSystemTime();

    // Copy cheevos from SPA -> GPD
    auto new_gpd = xdbf::GpdFile(title_data.title_id);
    auto title_gpd = &new_gpd;
    if (title_data.title_id == kDashboardID) {
      // we're loading dash - may as well update dash gpd
      title_gpd = &dash_gpd_;
    }

    for (auto ach : spa_achievements) {
      title_gpd->UpdateAchievement(ach);
      title_info.achievement_stats.achievements_count++;
      title_info.achievement_stats.gamerscore_total += ach.gamerscore;
    }

    // Try adding title image & name
    auto* title_image = spa_data.GetEntry(static_cast<xdbf::XdbfSection>(xdbf::SpaSection::kImage),
        static_cast<uint64_t>(xdbf::SpaID::Title));
    if (title_image) {
      title_gpd->UpdateEntry(*title_image);
    }

    auto title_name = xe::to_utf16(spa_data.GetTitleName());
    if (title_name.length()) {
      xdbf::Entry title_name_ent;
      title_name_ent.info.section = xdbf::XdbfSection::kString;
      title_name_ent.info.id =
          static_cast<uint64_t>(xdbf::SpaID::Title);
      title_name_ent.data.resize((title_name.length() + 1) * 2);
      xe::copy_and_swap((char16_t*)title_name_ent.data.data(),
                        title_name.c_str(), title_name.length());
      title_gpd->UpdateEntry(title_name_ent);
    }

    // Update dash GPD with title and write updated GPDs
    if (curr_title_id_ != kDashboardID) {
      title_gpds_[curr_title_id_] = *title_gpd;
      dash_gpd_.UpdateTitle(title_info);
      UpdateGpd(curr_title_id_, title_gpds_[curr_title_id_]);
    }
    UpdateGpd(kDashboardID, dash_gpd_);
  }

  curr_gpd_ = curr_title_id_ != kDashboardID ? &title_gpds_[curr_title_id_]
                                             : &dash_gpd_;

    std::vector<xdbf::Achievement> achievements;
  if (curr_gpd_->GetAchievements(&achievements)) {
    XELOGI("Achievement list:");

    for (auto ach : achievements) {
      // TODO: use ach.unachieved_desc for locked achievements?
      // depends on AchievementFlags::kShowUnachieved afaik
      XELOGI("{} - {} - {} - {} GS - {}", ach.id, xe::to_utf8(ach.label),
             xe::to_utf8(ach.description), ach.gamerscore,
             ach.IsUnlocked() ? "unlocked" : "locked");
    }

    XELOGI("Unlocked achievements: {}/{}, gamerscore: {}/{}\r\n",
           title_info.achievement_stats.achievements_earned,
           title_info.achievement_stats.achievements_count,
           title_info.achievement_stats.gamerscore_earned,
           title_info.achievement_stats.gamerscore_total);
  }

  return curr_gpd_;
}

xdbf::GpdFile* UserProfile::GetTitleGpd(const uint32_t title_id) {
  if (title_id == -1 || title_id == 0) {
    return curr_gpd_;
  }

  auto gpd = title_gpds_.find(title_id);
  if (gpd == title_gpds_.end()) {
    return nullptr;
  }

  return &(*gpd).second;
}

void UserProfile::GetTitles(std::vector<xdbf::GpdFile*>& titles) {
  for (auto title : title_gpds_) {
    titles.push_back(&title.second);
  }
}

bool UserProfile::UpdateTitleGpd(uint32_t title_id) {
  if (title_id == -1) {
    if (!curr_gpd_ || curr_title_id_ == -1) {
      return false;
    }
    title_id = curr_title_id_;
  }

  bool result = UpdateGpd(title_id, *curr_gpd_);
  if (!result) {
    XELOGE("UpdateTitleGpd failed on title {:08X}!", title_id);
  } else {
    XELOGD("Updated title {:08X} GPD successfully!", title_id);
  }
  return result;
}

bool UserProfile::UpdateAllGpds() {
  // TODO: optimize so we only have to update the current title?
  for (const auto& pair : title_gpds_) {
    auto gpd = pair.second;
    bool result = UpdateGpd(pair.first, gpd);
    if (!result) {
      XELOGE("UpdateGpdFiles failed on title %X...", pair.first);
      continue;
    }
  }

  // No need to update dash GPD here, the UpdateGpd func should take care of it
  // when needed
  return true;
}

xdbf::kAchievementsStats UserProfile::RecalculateAchievementsStats(
    const uint32_t title_id, xdbf::GpdFile& gpd_data) {

  xdbf::kAchievementsStats stats;
  // Update achievement total settings
  if (title_id != kDashboardID) {
    std::vector<xdbf::Achievement> gpd_achievements;
    gpd_data.GetAchievements(&gpd_achievements);

    for (auto ach : gpd_achievements) {
      stats.achievements_count++;
      stats.gamerscore_total += ach.gamerscore;
      if (ach.IsUnlocked()) {
        stats.achievements_earned++;
        stats.gamerscore_earned += ach.gamerscore;
      }
    }

    gpd_data.UpdateSetting(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED,
                      stats.achievements_count));
    gpd_data.UpdateSetting(xdbf::Setting(
        xdbf::XPROFILE_GAMERCARD_TITLE_CRED_EARNED, stats.gamerscore_earned));
  } else {
    // We're writing dash gpd, so recalculate total achievements
    // earned/gamerscore
    std::vector<xdbf::TitlePlayed> titles;
    dash_gpd_.GetTitles(&titles);
    for (auto title : titles) {
      stats.achievements_earned += title.achievement_stats.achievements_earned;
      stats.gamerscore_earned += title.achievement_stats.gamerscore_earned;
    }

    dash_gpd_.UpdateSetting(xdbf::Setting(
        xdbf::XPROFILE_GAMERCARD_TITLES_PLAYED, (uint32_t)titles.size()));
    dash_gpd_.UpdateSetting(xdbf::Setting(xdbf::XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED,
                      stats.achievements_earned));
    dash_gpd_.UpdateSetting(
        xdbf::Setting(xdbf::XPROFILE_GAMERCARD_CRED, stats.gamerscore_earned));
  }
  return stats;
}

bool UserProfile::UpdateGpd(uint32_t title_id, xdbf::GpdFile& gpd_data) {
  xdbf::kAchievementsStats recalculated_stats =
      RecalculateAchievementsStats(title_id, gpd_data);

  size_t gpd_length = 0;
  if (!gpd_data.Write(nullptr, &gpd_length)) {
    XELOGE("Failed to get GPD size for %X!", title_id);
    return false;
  }

  // Open profile and update shit!
  const std::string guest_path = fmt::format("{:016X}", offline_xuid) + ":\\" +
                                 fmt::format("{:08X}.gpd", title_id);

  xe::vfs::File* output_file;
  xe::vfs::FileAction action = {};
  auto status = kernel_state()->file_system()->OpenFile(
      nullptr, guest_path, xe::vfs::FileDisposition::kSuperscede,
      xe::vfs::FileAccess::kGenericAll, false, true, &output_file, &action);

  std::vector<uint8_t> gpd_file_data;
  gpd_file_data.resize(gpd_length);
  gpd_data.Write(gpd_file_data.data(), &gpd_length);

  size_t written_bytes = 0;
  output_file->WriteSync(gpd_file_data.data(), gpd_length, 0, &written_bytes);
  output_file->Destroy(); // Close&Save!

  // Check if we need to update dashboard data...
  if (title_id != kDashboardID) {
    xdbf::TitlePlayed title_info;
    if (dash_gpd_.GetTitle(title_id, &title_info)) {
      if (recalculated_stats.isStatsUpdateRequired(title_info.achievement_stats)) {
        title_info.achievement_stats = recalculated_stats;
      }
      dash_gpd_.UpdateTitle(title_info);
      UpdateGpd(kDashboardID, dash_gpd_);
    }
  }
  return true;
}

bool UserProfile::AddSettingIfNotExist(xdbf::Setting& setting) {
  if (dash_gpd_.GetSetting(setting.id, nullptr)) {
    return false;
  }
  if (setting.value.type == xdbf::X_XUSER_DATA_TYPE::kBinary &&
      !setting.extraData.size()) {
    setting.extraData.resize(XPROFILEID_SIZE(setting.id));
  }
  return dash_gpd_.UpdateSetting(setting);
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe
