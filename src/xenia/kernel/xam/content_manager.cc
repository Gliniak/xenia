/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/content_manager.h"

#include <string>

#include "third_party/fmt/include/fmt/format.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/string.h"
#include "xenia/emulator.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/user_module.h"
#include "xenia/kernel/xobject.h"
#include "xenia/vfs/devices/stfs_container_device.h"

namespace xe {
namespace kernel {
namespace xam {

static const char* kThumbnailFileName = "__thumbnail.png";

static const char* kGameUserContentDirName = "profile";

static int content_device_id_ = 0;

ContentPackage::ContentPackage(KernelState* kernel_state,
                               const std::string_view root_name,
                               const XCONTENT_AGGREGATE_DATA& data,
                               const uint32_t flags,
                               const std::filesystem::path& package_path,
                               bool read_only)
    : kernel_state_(kernel_state), root_name_(root_name) {
  device_path_ = fmt::format("\\Device\\Content\\{0}\\", ++content_device_id_);
  content_data_ = data;

  auto fs = kernel_state_->file_system();
  auto device = std::make_unique<vfs::StfsContainerDevice>(
      device_path_, package_path, read_only);

  // Normally on console before first write empty package already exist
  // Which means that it is initialy created
  if (!read_only && !std::filesystem::exists(package_path)) {
    device->CreateStfsContainer(data, flags);
  }

  device->Initialize();
  fs->RegisterDevice(std::move(device));
  fs->RegisterSymbolicLink(root_name_ + ":", device_path_);
}

ContentPackage::~ContentPackage() {
  auto fs = kernel_state_->file_system();
  fs->UnregisterSymbolicLink(root_name_ + ":");
  fs->UnregisterDevice(device_path_);
}

std::vector<uint8_t> ContentPackage::GetThumbnail() {
  std::vector<uint8_t> thumbnail = {};

  auto device = reinterpret_cast<vfs::StfsContainerDevice*>(
      kernel_state_->file_system()->ResolveDevice(device_path_));
  if (!device) {
    return thumbnail;
  }

  thumbnail.reserve(device->header().metadata.thumbnail_size);
  std::memcpy(thumbnail.data(), device->header().metadata.thumbnail,
              device->header().metadata.thumbnail_size);

  return thumbnail;
}

void ContentPackage::SetThumbnail(std::vector<uint8_t> thumbnail) {
  auto device = reinterpret_cast<vfs::StfsContainerDevice*>(
      kernel_state_->file_system()->ResolveDevice(device_path_));
  if (!device) {
    return;
  }

  const uint32_t thumb_length = std::min(
      (uint32_t)thumbnail.size(), vfs::XContentPackageMetadata::kThumbLengthV2);

  std::memcpy(device->header().metadata.thumbnail, thumbnail.data(),
              thumbnail.size());
  device->header().metadata.thumbnail_size = thumb_length;
}

ContentManager::ContentManager(KernelState* kernel_state,
                               const std::filesystem::path& root_path)
    : kernel_state_(kernel_state), root_path_(root_path) {}

ContentManager::~ContentManager() = default;

std::filesystem::path ContentManager::ResolvePackageRoot(
    XContentType content_type, uint32_t title_id) {
  if (title_id == kCurrentlyRunningTitleId) {
    title_id = kernel_state_->title_id();
  }
  auto title_id_str = fmt::format("{:08X}", title_id);
  auto content_type_str = fmt::format("{:08X}", uint32_t(content_type));

  // Package root path:
  // content_root/title_id/content_type/
  return root_path_ / title_id_str / content_type_str;
}

std::filesystem::path ContentManager::ResolvePackagePath(
    const XCONTENT_AGGREGATE_DATA& data) {
  // Content path:
  // content_root/title_id/content_type/data_file_name/
  auto package_root = ResolvePackageRoot(data.content_type, data.title_id);
  return package_root / xe::to_path(data.file_name());
}

std::vector<XCONTENT_AGGREGATE_DATA> ContentManager::ListContent(
    uint32_t device_id, XContentType content_type, uint32_t title_id) {
  std::vector<XCONTENT_AGGREGATE_DATA> result;

  if (title_id == kCurrentlyRunningTitleId) {
    title_id = kernel_state_->title_id();
  }

  // Search path:
  // content_root/title_id/content_type/*
  auto package_root = ResolvePackageRoot(content_type, title_id);
  auto file_infos = xe::filesystem::ListFiles(package_root);
  for (const auto& file_info : file_infos) {
    if (file_info.type != xe::filesystem::FileInfo::Type::kFile) {
      // Files only.
      continue;
    }
    if (file_info.total_size < sizeof(vfs::XContentHeader)) {
      // Too small to be valid package
      continue;
    }

    auto file_path = file_info.path / file_info.name;

    // Check file magic before reading with StfsContainerDevice...
    auto file = xe::filesystem::OpenFile(file_path, "rb");
    vfs::XContentPackageHeader header;
    auto read = fread(&header, sizeof(header), 1, file);
    fclose(file);

    if (!read || !header.is_magic_valid()) {
      // Invalid file magic
      continue;
    }

    // Open device as read-only so that flushing etc isn't performed
    auto device = std::make_unique<vfs::StfsContainerDevice>(
        fmt::format("\\Device\\Content\\{0}\\", ++content_device_id_),
        file_path, true);
    if (!device->Initialize()) {
      --content_device_id_;
      // Error reading as STFS package
      continue;
    }

    XCONTENT_AGGREGATE_DATA content_data;
    content_data.device_id = device_id;
    content_data.content_type = device->header().metadata.content_type;

    // Get display name in the titles default language, as some JP games seem to
    // expect the japanese display_name value
    content_data.set_display_name(device->header().metadata.display_name(
        kernel_state_->emulator()->title_language()));

    content_data.set_file_name(path_to_utf8(file_info.name));
    content_data.title_id = title_id;

    result.emplace_back(std::move(content_data));
  }

  return result;
}

std::unique_ptr<ContentPackage> ContentManager::ResolvePackage(
    const std::string_view root_name, const XCONTENT_AGGREGATE_DATA& data,
    uint32_t flags, bool read_only, bool create) {
  auto package_path = ResolvePackagePath(data);
  if (!create && !std::filesystem::exists(package_path)) {
    return nullptr;
  }

  auto global_lock = global_critical_region_.Acquire();

  auto package = std::make_unique<ContentPackage>(
      kernel_state_, root_name, data, flags, package_path, read_only);
  return package;
}

bool ContentManager::ContentExists(const XCONTENT_AGGREGATE_DATA& data) {
  auto path = ResolvePackagePath(data);
  return std::filesystem::exists(path);
}

X_RESULT ContentManager::CreateContent(const std::string_view root_name,
                                       const XCONTENT_AGGREGATE_DATA& data,
                                       uint32_t flags) {
  auto global_lock = global_critical_region_.Acquire();

  if (open_packages_.count(string_key(root_name))) {
    // Already content open with this root name.
    return X_ERROR_ALREADY_EXISTS;
  }

  auto package_path = ResolvePackagePath(data);
  if (std::filesystem::exists(package_path)) {
    // Exists, must not!
    return X_ERROR_ALREADY_EXISTS;
  }

  auto parent = package_path.parent_path();
  std::filesystem::create_directories(parent);
  if (!std::filesystem::exists(parent)) {
    // Failed to create parent path?
    return X_ERROR_ACCESS_DENIED;
  }

  auto package = ResolvePackage(root_name, data, flags, false, true);
  assert_not_null(package);

  open_packages_.insert({string_key::create(root_name), package.release()});

  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::OpenContent(const std::string_view root_name,
                                     const XCONTENT_AGGREGATE_DATA& data) {
  auto global_lock = global_critical_region_.Acquire();

  if (open_packages_.count(string_key(root_name))) {
    // Already content open with this root name.
    return X_ERROR_ALREADY_EXISTS;
  }

  auto package_path = ResolvePackagePath(data);
  if (!std::filesystem::exists(package_path)) {
    // Does not exist, must be created.
    return X_ERROR_FILE_NOT_FOUND;
  }

  // Open package.
  auto package = ResolvePackage(root_name, data);
  assert_not_null(package);

  open_packages_.insert({string_key::create(root_name), package.release()});

  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::CloseContent(const std::string_view root_name) {
  auto global_lock = global_critical_region_.Acquire();

  auto it = open_packages_.find(string_key(root_name));
  if (it == open_packages_.end()) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  auto package = it->second;
  open_packages_.erase(it);
  delete package;

  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::GetContentThumbnail(
    const XCONTENT_AGGREGATE_DATA& data, std::vector<uint8_t>* buffer) {
  auto global_lock = global_critical_region_.Acquire();
  auto package_path = ResolvePackagePath(data);
  if (!std::filesystem::exists(package_path)) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  auto package =
      std::find_if(open_packages_.cbegin(), open_packages_.cend(),
                   [data](std::pair<string_key, ContentPackage*> content) {
                     return data == content.second->GetPackageContentData();
                   });

  if (package != std::end(open_packages_)) {
    // Package was found in open_packages_
    auto thumbnail = package->second->GetThumbnail();
    auto thumb_length = std::min((uint32_t)thumbnail.size(),
                                 vfs::XContentPackageMetadata::kThumbLengthV2);
    buffer->resize(thumb_length);
    memcpy(buffer->data(), thumbnail.data(), thumb_length);
    return X_ERROR_SUCCESS;
  }

  auto file = xe::filesystem::OpenFile(package_path, "rb");
  auto header = std::make_unique<vfs::XContentHeader>();
  if (fread(header.get(), sizeof(vfs::XContentHeader), 1, file) != 1) {
    fclose(file);
    return X_ERROR_FILE_NOT_FOUND;
  }
  auto thumb_size = std::min(uint32_t(header->metadata.thumbnail_size),
                             vfs::XContentPackageMetadata::kThumbLengthV2);
  buffer->resize(thumb_size);
  memcpy(const_cast<uint8_t*>(buffer->data()), header->metadata.thumbnail,
         thumb_size);

  fclose(file);
  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::SetContentThumbnail(
    const XCONTENT_AGGREGATE_DATA& data, std::vector<uint8_t> buffer) {
  auto global_lock = global_critical_region_.Acquire();
  auto package_path = ResolvePackagePath(data);
  if (!std::filesystem::exists(package_path)) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  auto package =
      std::find_if(open_packages_.cbegin(), open_packages_.cend(),
                   [data](std::pair<string_key, ContentPackage*> content) {
                     return data == content.second->GetPackageContentData();
                   });

  if (package != std::end(open_packages_)) {
    // Package was found in open_packages_
    package->second->SetThumbnail(buffer);
    return X_ERROR_SUCCESS;
  }

  // Package isn't loaded atm
  //
  // TODO: in future this will probably need to create an StfsContainerDevice
  // and update thumb through that, so header hashes etc are updated
  // Xenia doesn't care about those hashes though, but it's important for
  // console support

  auto file = xe::filesystem::OpenFile(package_path, "rb+");
  auto header = std::make_unique<vfs::XContentHeader>();
  if (fread(header.get(), sizeof(vfs::XContentHeader), 1, file) != 1) {
    fclose(file);
    return X_ERROR_FILE_NOT_FOUND;
  }

  auto thumb_size = std::min(uint32_t(buffer.size()),
                             vfs::XContentPackageMetadata::kThumbLengthV2);

  header->metadata.thumbnail_size = thumb_size;
  memcpy(header->metadata.thumbnail, buffer.data(), thumb_size);
  fseek(file, 0, SEEK_SET);
  fwrite(header.get(), sizeof(*header), 1, file);
  fclose(file);
  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::DeleteContent(const XCONTENT_AGGREGATE_DATA& data) {
  auto global_lock = global_critical_region_.Acquire();

  if (IsContentOpen(data)) {
    // TODO(Gliniak): Get real error code for this case.
    return X_ERROR_ACCESS_DENIED;
  }

  auto package_path = ResolvePackagePath(data);
  if (std::filesystem::remove_all(package_path) > 0) {
    return X_ERROR_SUCCESS;
  } else {
    return X_ERROR_FILE_NOT_FOUND;
  }
}

std::filesystem::path ContentManager::ResolveGameUserContentPath() {
  auto title_id_str = fmt::format("{:08X}", kernel_state_->title_id());
  auto user_name = xe::to_path(kernel_state_->user_profile()->name());

  // Per-game per-profile data location:
  // content_root/title_id/profile/user_name
  return root_path_ / title_id_str / kGameUserContentDirName / user_name;
}

bool ContentManager::IsContentOpen(const XCONTENT_AGGREGATE_DATA& data) const {
  return std::any_of(open_packages_.cbegin(), open_packages_.cend(),
                     [data](std::pair<string_key, ContentPackage*> content) {
                       return data == content.second->GetPackageContentData();
                     });
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe
