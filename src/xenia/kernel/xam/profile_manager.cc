/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/profile_manager.h"

#include <filesystem>

#include "third_party/fmt/include/fmt/format.h"
#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/crypto_utils.h"
#include "xenia/vfs/devices/stfs_container_device.h"

DEFINE_string(logged_profile_xuid, "FFFFFFFFFFFFFFFE",
              "XUID of the profile to load on boot in (E0...)", "XAM");

namespace xe {
namespace kernel {
namespace xam {

bool ProfileManager::DecryptAccountFile(const uint8_t* data,
                                        X_XAMACCOUNTINFO* output, bool devkit) {
  const uint8_t* key = util::GetXeKey(0x19, devkit);
  if (!key) {
    return false;  // this shouldn't happen...
  }

  // Generate RC4 key from data hash
  uint8_t rc4_key[0x14];
  util::HmacSha(key, 0x10, data, 0x10, 0, 0, 0, 0, rc4_key, 0x14);

  uint8_t dec_data[sizeof(X_XAMACCOUNTINFO) + 8];

  // Decrypt data
  util::RC4(rc4_key, 0x10, data + 0x10, sizeof(dec_data), dec_data,
            sizeof(dec_data));

  // Verify decrypted data against hash
  uint8_t data_hash[0x14];
  util::HmacSha(key, 0x10, dec_data, sizeof(dec_data), 0, 0, 0, 0, data_hash,
                0x14);

  if (std::memcmp(data, data_hash, 0x10) == 0) {
    // Copy account data to output
    std::memcpy(output, dec_data + 8, sizeof(X_XAMACCOUNTINFO));

    // Swap gamertag endian
    xe::copy_and_swap<char16_t>(output->gamertag, output->gamertag, 0x10);
    return true;
  }

  return false;
}

void ProfileManager::EncryptAccountFile(const X_XAMACCOUNTINFO* input,
                                        uint8_t* output, bool devkit) {
  const uint8_t* key = util::GetXeKey(0x19, devkit);
  if (!key) {
    return;  // this shouldn't happen...
  }

  X_XAMACCOUNTINFO* output_acct = (X_XAMACCOUNTINFO*)(output + 0x18);
  std::memcpy(output_acct, input, sizeof(X_XAMACCOUNTINFO));

  // Swap gamertag endian
  xe::copy_and_swap<char16_t>(output_acct->gamertag, output_acct->gamertag,
                              0x10);

  // Set confounder, should be random but meh
  std::memset(output + 0x10, 0xFD, 8);

  // Encrypted data = xam account info + 8 byte confounder
  uint32_t enc_data_size = sizeof(X_XAMACCOUNTINFO) + 8;

  // Set data hash
  uint8_t data_hash[0x14];
  util::HmacSha(key, 0x10, output + 0x10, enc_data_size, 0, 0, 0, 0, data_hash,
                0x14);

  std::memcpy(output, data_hash, 0x10);

  // Generate RC4 key from data hash
  uint8_t rc4_key[0x14];
  util::HmacSha(key, 0x10, data_hash, 0x10, 0, 0, 0, 0, rc4_key, 0x14);

  // Encrypt data
  util::RC4(rc4_key, 0x10, output + 0x10, enc_data_size, output + 0x10,
            enc_data_size);
}

ProfileManager::ProfileManager(KernelState* kernel_state,
                               const std::filesystem::path& profiles_path)
    : kernel_state_(kernel_state), profiles_path_(profiles_path) {
  logged_profiles_.clear();
  profiles_.clear();

  LoadAccounts(FindProfiles());
}

ProfileManager::~ProfileManager() {}

UserProfile* ProfileManager::GetProfileWithUserIndex(uint8_t user_index) {
  uint8_t actual_user_index = user_index;
  if (user_index == 0xFF) {
    actual_user_index = 0;
  }

  for (auto const entry : logged_profiles_) {
    if (entry.first == user_index) {
      return entry.second;
    }
  }
  return nullptr;
}

UserProfile* ProfileManager::GetProfileWithXuid(uint64_t xuid,
                                                bool include_not_signed) {
  for (auto const entry : logged_profiles_) {
    if (entry.second->xuid() == xuid) {
      return entry.second;
    }
  }

  if (include_not_signed) {
    for (auto const entry : profiles_) {
      if (entry.second->xuid() == xuid) {
        return entry.second;
      }
    }
  }
  return nullptr;
}

bool ProfileManager::MountProfile(const std::string xuid,
                                  const std::string mount_path,
                                  bool read_only) {
  std::filesystem::path stfs_profile_path = GetProfilePath(xuid);

  std::string guest_mount_path = xuid + ':';
  if (!mount_path.empty()) {
    guest_mount_path = mount_path + ':';
  }

  auto device = std::make_unique<vfs::StfsContainerDevice>(
      guest_mount_path, stfs_profile_path, read_only);
  if (!device->Initialize()) {
    XELOGE(
        "MountProfile: Unable to mount {} as STFS; file not found or "
        "corrupt.",
        stfs_profile_path.u8string());
    return false;
  }
  return kernel_state_->file_system()->RegisterDevice(std::move(device));
}

bool ProfileManager::CreateAccount(const std::filesystem::path profile_path,
                                   const uint64_t xuid,
                                   const X_XAMACCOUNTINFO* account) {
  auto guest_path = profile_path.filename().u8string() + "://Account";
  XELOGI("ProfileManager: Creating Account: {}", profile_path.u8string());

  xe::vfs::FileAction action = {};
  xe::vfs::File* output_file;
  auto status = kernel_state_->file_system()->OpenFile(
      nullptr, guest_path, xe::vfs::FileDisposition::kCreate,
      xe::vfs::FileAccess::kFileWriteData, false, true, &output_file, &action);

  if (XFAILED(status)) {
    return false;
  }

  std::vector<uint8_t> data;
  data.resize(sizeof(X_XAMACCOUNTINFO) + 0x18);
  EncryptAccountFile(account, data.data());

  size_t written_bytes = 0;
  output_file->WriteSync(data.data(), data.size(), 0, &written_bytes);
  output_file->Destroy();
  return true;
}

bool ProfileManager::LoadAccount(const std::filesystem::path profile_path) {
  // If Account doesn't exist then profile SHOULDN'T exist!
  XELOGI("ProfileManager: Loading Account: {}", profile_path.u8string());

  MountProfile(profile_path.u8string(), "", true);

  X_XAMACCOUNTINFO tmp_acct;  // ToDo: Create deault parameters?

  const std::string guest_path = profile_path.u8string() + ":\\Account";

  xe::vfs::File* output_file;
  xe::vfs::FileAction action = {};
  auto status = kernel_state_->file_system()->OpenFile(
      nullptr, guest_path, xe::vfs::FileDisposition::kOpen,
      xe::vfs::FileAccess::kFileReadData, false, true, &output_file, &action);

  if (XFAILED(status)) {
    DismountProfile(profile_path.u8string());
    return false;
  }

  if (output_file && output_file->entry()) {
    XELOGI("Loading Account file from path {}", guest_path);
    std::vector<uint8_t> file_data;
    file_data.resize(output_file->entry()->size());

    size_t bytes_read = 0;
    output_file->ReadSync(file_data.data(), output_file->entry()->size(), 0,
                          &bytes_read);

    bool success =
        ProfileManager::DecryptAccountFile(file_data.data(), &tmp_acct);
    if (!success) {
      success =
          ProfileManager::DecryptAccountFile(file_data.data(), &tmp_acct, true);
    }
    if (!success) {
      XELOGW("Failed to decrypt Account file data");
      return false;
    }
  }

  // We don't need profile to be mounted anymore, so for now let's close it.
  // We need it only when we want to login into this account!
  DismountProfile(profile_path.u8string());

  const uint64_t xuid = std::stoull(profile_path.u8string(), nullptr, 16);
  profiles_.insert({profile_path.u8string(), new UserProfile(xuid, &tmp_acct)});
  return true;
}

void ProfileManager::LoadAccounts(
    const std::vector<std::string> profiles_paths) {
  for (const auto& path : profiles_paths) {
    LoadAccount(path);
  }
}

bool ProfileManager::MountProfile(const uint64_t xuid,
                                  const std::string mount_path) {
  return MountProfile(fmt::format("{:016X}", xuid), mount_path);
}

bool ProfileManager::DismountProfile(const std::string xuid) {
  return kernel_state_->file_system()->UnregisterDevice(xuid + ':');
}

void ProfileManager::Login(uint64_t xuid) {
  if (logged_profiles_.size() > 4) {
    XELOGE(
        "Cannot login account with XUID: {:016X} due to lack of free login "
        "slots (Max 4 accounts at once)",
        xuid);
    return;
  }

  if (xuid == -1) {
    xuid = std::stoull(cvars::logged_profile_xuid, nullptr, 16);
  }
  const std::string xuid_string = fmt::format("{:016X}", xuid);

  for (const auto entry : profiles_) {
    if (entry.first != xuid_string) {
      continue;
    }

    // Change to r/w when issues with gpd will be resolved!
    if (!MountProfile(entry.first, "", false)) {
      continue;
    }

    xam::xdbf::GpdFile dash_gpd = ReadProfileDashboardGpd(entry.first);
    auto titles_gpds = ReadProfileTitlesGpd(entry.first, dash_gpd);

    UserProfile* profile =
        new UserProfile(xuid, &entry.second->account(), dash_gpd, titles_gpds);

    XELOGI("Loaded {} (GUID: {:016X}), as a default user", profile->name(),
           profile->xuid());
    logged_profiles_[FindFirstFreeProfileSlot()] = profile;
    kernel_state_->BroadcastNotification(0x0000000A, GetUsedUserSlots());
    OVERRIDE_string(logged_profile_xuid, xuid_string);
  }
}

void ProfileManager::Logout(const uint64_t xuid) {
  DismountProfile(fmt::format("{:016X}", xuid));

  for (std::map<uint8_t, UserProfile*>::const_iterator itr =
           logged_profiles_.cbegin();
       itr != logged_profiles_.cend(); itr++) {
    if (itr->second->xuid() != xuid) {
      continue;
    }

    logged_profiles_.erase(itr);
    kernel_state_->BroadcastNotification(0x0000000A, GetUsedUserSlots());
    break;
  }
}

bool ProfileManager::CreateProfile(const X_XAMACCOUNTINFO* account) {
  // ToDo: Write more robust xuid generator!
  uint64_t xuid = GenerateXuid();
  std::filesystem::path profile_path = GetProfilePath(xuid);

  std::filesystem::create_directories(profile_path.parent_path());

  XCONTENT_AGGREGATE_DATA profile_data = {0};
  profile_data.content_type = XContentType::kProfile;
  profile_data.device_id = 0;
  profile_data.title_id = kDashboardID;
  profile_data.profile_xuid = xuid;
  profile_data.set_file_name(profile_path.filename().u8string());

  auto create_package_result = kernel_state_->content_manager()->CreateContent(
      profile_path.parent_path().u8string(), profile_data, 0);

  if (XFAILED(create_package_result)) {
    return false;
  }

  kernel_state_->content_manager()->CloseContent(
      profile_path.parent_path().u8string());
  // filesystem::CreateEmptyFile(profile_path);

  MountProfile(fmt::format("{:016X}", xuid), "", false);
  // Files that should be initially created in profile package:
  // - Account - Done
  // - dashboard gpd - ToDo (It should be created AFTER restart!
  // Normally it seems like creating profile should create dashboard gpd file,
  // but it might not be a case normally. On console you have to go through
  // dashboard everytime
  // - profile thumbnail/icon - ToDo

  // STRUCTURE IS STILL INVALID!!!

  bool result = CreateAccount(profile_path, xuid, account);

  // Create dashboard GPD (Settings only!)
  xdbf::GpdFile* dashboard_gpd = new xdbf::GpdFile(kDashboardID);

  dashboard_gpd->UpdateSetting(
      xdbf::Setting(xdbf::X_XDBF_SETTING_ID::XPROFILE_GAMERCARD_USER_NAME,
                    xe::to_utf16(account->GetGamertagString())));

  std::vector<uint8_t> data;
  data.resize(dashboard_gpd->size());

  size_t ds = 0;
  dashboard_gpd->Write(data.data(), &ds);

  // Create tile_64, tile_32 pngs - Unknown from where it is being taken

  if (result) {
    profiles_.insert({profile_path.u8string(), new UserProfile(xuid, account)});

    if (IsAnyProfileSignedIn()) {
      for (auto& entry : logged_profiles_) {
        Logout(entry.second->xuid());
      }
    }
  } else {
    XELOGE("Cannot create profile! WTF");
  }

  return result;
}

std::vector<std::string> ProfileManager::FindProfiles() {
  // Info: Profile directory name is also it's offline xuid
  std::vector<std::string> profiles_directories;

  auto profiles_directory = xe::filesystem::ListFilesWithPattern(
      profiles_path_, std::regex("[0-9A-F]{16}"));

  for (const auto profile : profiles_directory) {
    if (!std::filesystem::exists(
            profile.path / profile.name / kDashboardStringID /
            fmt::format("{:08X}", XContentType::kProfile) / profile.name)) {
      XELOGE("Profile {} doesn't have profile package!",
             profile.name.u8string());
      continue;
    }

    XELOGE("Adding profile {} to profile list", profile.name.u8string());
    profiles_directories.push_back(profile.name.u8string());
  }

  XELOGE("ProfileManager: Found {} Profiles", profiles_directories.size());
  return profiles_directories;
}

uint8_t ProfileManager::FindFirstFreeProfileSlot() const {
  uint8_t free_slot = 0;
  if (!IsAnyProfileSignedIn()) {
    return free_slot;
  }

  for (const auto entry : logged_profiles_) {
    if (entry.second == nullptr) {
      return free_slot;
    }
    free_slot++;
  }
  return -1;
}

uint8_t ProfileManager::GetUsedUserSlots() const {
  uint8_t used_slots = 0;

  uint8_t i = 0;
  for (const auto entry : logged_profiles_) {
    if (entry.second == nullptr) {
      i++;
      continue;
    }
    used_slots = used_slots + (1 << i);
    i++;
  }

  assert_true(used_slots < 0xF);
  return used_slots & 0xF;
}

xam::xdbf::GpdFile ProfileManager::ReadProfileDashboardGpd(
    const std::string xuid) {
  xam::xdbf::GpdFile dash_gpd(kDashboardID);
  if (ReadGPD(xuid, kDashboardID, dash_gpd)) {
  } else {
    XELOGW("Failed to read dash GPD (FFFE07D1.gpd), using blank one");
    // Create empty settings syncdata, helps tools identify this XDBF as a GPD
    xdbf::Entry ent;
    ent.info.section = xdbf::XdbfSection::kSetting;
    ent.info.id = 0x200000000;
    ent.data.resize(0x18);
    memset(ent.data.data(), 0, 0x18);
    dash_gpd.UpdateEntry(ent);
  }
  return dash_gpd;
}

std::unordered_map<uint32_t, xdbf::GpdFile>
ProfileManager::ReadProfileTitlesGpd(const std::string xuid,
                                     const xam::xdbf::GpdFile dash_gpd) {
  std::unordered_map<uint32_t, xdbf::GpdFile> profile_titles;
  std::vector<xam::xdbf::TitlePlayed> titles;
  dash_gpd.GetTitles(&titles);

  for (auto title : titles) {
    xdbf::GpdFile title_gpd(title.title_id);

    if (ReadGPD(xuid, title.title_id, title_gpd)) {
      profile_titles[title.title_id] = title_gpd;
    }
  }
  XELOGI("Loaded {} profile GPDs", profile_titles.size());
  return profile_titles;
}

bool ProfileManager::ReadGPD(const std::string xuid, const uint32_t title_id,
                             xam::xdbf::GpdFile& gpd) {
  const std::string guest_path = xuid + fmt::format(":\\{:08X}.gpd", title_id);

  xe::vfs::File* output_file;
  xe::vfs::FileAction action = {};
  auto status = kernel_state_->file_system()->OpenFile(
      nullptr, guest_path, xe::vfs::FileDisposition::kOpen,
      xe::vfs::FileAccess::kFileReadData, false, true, &output_file, &action);

  if (XFAILED(status)) {
    return false;
  }

  if (!output_file->entry()) {
    return false;
  }

  std::vector<uint8_t> file_data;
  file_data.resize(output_file->entry()->size());

  size_t bytes_read = 0;
  output_file->ReadSync(file_data.data(), output_file->entry()->size(), 0,
                        &bytes_read);

  return gpd.Read(file_data.data(), file_data.size());
}

std::filesystem::path ProfileManager::GetProfilePath(const uint64_t xuid) {
  std::string xuid_string = fmt::format("{:016X}", xuid);
  return GetProfilePath(xuid_string);
}

uint8_t ProfileManager::GetUserIndexAssignedToProfile(const uint64_t xuid) {
  uint8_t slot = -1;

  for (std::map<uint8_t, UserProfile*>::const_iterator itr =
           logged_profiles_.cbegin();
       itr != logged_profiles_.cend(); itr++) {
    if (itr->second->xuid() == xuid) {
      return slot + 1;
    }
    slot++;
  }
  return -1;
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe