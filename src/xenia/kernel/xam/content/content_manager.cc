/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/content/content_manager.h"
#include <array>
#include <string>
#include <unordered_set>
#include "xenia/kernel/xam/content/host_content_package.h"
#include "xenia/kernel/xam/content/xcontent_package.h"

#include "third_party/fmt/include/fmt/format.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/string.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/xam/user_profile.h"
#include "xenia/kernel/xfile.h"
#include "xenia/kernel/xobject.h"
#include "xenia/vfs/devices/host_path_device.h"

extern "C" {
#include "third_party/FFmpeg/libavutil/md5.h"
}

namespace xe {
namespace kernel {
namespace xam {

static const char* kGameUserContentDirName = "profile";

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
    const XCONTENT_AGGREGATE_DATA& data, const uint32_t disc_number) {
  // Content path:
  // content_root/title_id/content_type/data_file_name/
  auto get_package_path = [&, data, disc_number](const uint32_t title_id) {
    auto package_root = ResolvePackageRoot(data.content_type, title_id);
    std::string final_name = xe::string_util::trim(data.file_name());
    std::filesystem::path package_path = package_root / xe::to_path(final_name);

    if (disc_number != -1) {
      package_path /= fmt::format("disc{:03}", disc_number);
    }
    return package_path;
  };

  if (data.content_type == XContentType::kPublisher) {
    const std::unordered_set<uint32_t> title_ids =
        FindPublisherTitleIds(data.title_id);

    for (const auto& title_id : title_ids) {
      auto package_path = get_package_path(title_id);

      if (!std::filesystem::exists(package_path)) {
        continue;
      }
      return package_path;
    }
  }

  // Default handling for current title
  return get_package_path(data.title_id);
}

std::unordered_set<uint32_t> ContentManager::FindPublisherTitleIds(
    uint32_t base_title_id) const {
  if (base_title_id == kCurrentlyRunningTitleId) {
    base_title_id = kernel_state_->title_id();
  }
  std::unordered_set<uint32_t> title_ids = {};

  std::string publisher_id_regex =
      fmt::format("^{:04X}.*", static_cast<uint16_t>(base_title_id >> 16));
  // Get all publisher entries
  auto publisher_entries =
      xe::filesystem::FilterByName(xe::filesystem::ListDirectories(root_path_),
                                   std::regex(publisher_id_regex));

  for (const auto& entry : publisher_entries) {
    std::filesystem::path path_to_publisher_dir =
        entry.path / entry.name /
        fmt::format("{:08X}", XContentType::kPublisher);

    if (!std::filesystem::exists(path_to_publisher_dir)) {
      continue;
    }

    title_ids.insert(xe::string_util::from_string<uint32_t>(
        xe::path_to_utf8(entry.name), true));
  }

  // Always remove current title. It will be handled differently
  if (title_ids.count(base_title_id)) {
    title_ids.erase(base_title_id);
  }
  return title_ids;
}

std::vector<XCONTENT_AGGREGATE_DATA> ContentManager::ListContent(
    const uint32_t device_id, const XContentType content_type,
    uint32_t title_id) {
  std::vector<XCONTENT_AGGREGATE_DATA> result;

  if (title_id == kCurrentlyRunningTitleId) {
    title_id = kernel_state_->title_id();
  }

  std::unordered_set<uint32_t> title_ids = {title_id};

  if (content_type == XContentType::kPublisher) {
    title_ids = FindPublisherTitleIds(title_id);
  }

  for (const uint32_t& title_id : title_ids) {
    // Search path:
    // content_root/title_id/type_name/*
    auto package_root = ResolvePackageRoot(content_type, title_id);
    auto file_infos = xe::filesystem::ListFiles(package_root);

    for (const auto& file_info : file_infos) {
      const auto type = ResolvePackageType(file_info.path / file_info.name);

      switch (type) {
        case ContentPackage::PackageType::host: {
          HostContentPackage package =
              HostContentPackage(kernel_state_, file_info.path / file_info.name,
                                 device_id, title_id, content_type);

          result.emplace_back(*package.GetPackageContentData());
        }; break;
        case ContentPackage::PackageType::xcontent: {
          XContentPackage package =
              XContentPackage(kernel_state_, file_info.path / file_info.name);

          result.emplace_back(*package.GetPackageContentData());
        }; break;
        default:
          break;
      }
    }
  }
  return result;
}

ContentPackage::PackageType ContentManager::ResolvePackageType(
    const std::filesystem::path path) const {
  xe::filesystem::FileInfo info;

  const bool success = xe::filesystem::GetInfo(path, &info);
  if (!success) {
    return ContentPackage::PackageType::unknown;
  }

  switch (info.type) {
    case xe::filesystem::FileInfo::Type::kFile:
      return ContentPackage::PackageType::xcontent;
      break;
    case xe::filesystem::FileInfo::Type::kDirectory:
      return ContentPackage::PackageType::host;
      break;
    default:
      break;
  }
  return ContentPackage::PackageType::unknown;
}

std::unique_ptr<ContentPackage> ContentManager::ResolvePackage(
    const XCONTENT_AGGREGATE_DATA& data, const uint32_t disc_number) {
  auto package_path = ResolvePackagePath(data, disc_number);
  if (!std::filesystem::exists(package_path)) {
    return nullptr;
  }

  auto global_lock = global_critical_region_.Acquire();

  switch (ResolvePackageType(package_path)) {
    case ContentPackage::PackageType::host:
      return std::make_unique<HostContentPackage>(kernel_state_, package_path,
                                                  &data);
      break;
    case ContentPackage::PackageType::xcontent:
      return std::make_unique<XContentPackage>(kernel_state_, package_path,
                                               &data);
      break;
    default:
      break;
  }
  return nullptr;
}

bool ContentManager::ContentExists(const XCONTENT_AGGREGATE_DATA& data) {
  auto path = ResolvePackagePath(data);
  return std::filesystem::exists(path);
}

std::string ContentManager::GenerateUniquePackageId(
    const std::filesystem::path path) {
  AVMD5* md5 = av_md5_alloc();
  av_md5_init(md5);

  std::string path_as_string = xe::path_to_utf8(path);
  av_md5_update(md5, (const uint8_t*)(path_as_string.c_str()),
                (int)path_as_string.length());
  uint8_t digest[16];
  av_md5_final(md5, digest);

  std::string unique_id = "";
  for (uint8_t i = 0; i < 16; i++) {
    unique_id += fmt::format("{:02X}", digest[i]);
  }
  return unique_id;
}

X_RESULT ContentManager::CreateContent(const std::string_view root_name,
                                       const XCONTENT_AGGREGATE_DATA& data,
                                       const uint32_t flags) {
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

  if (!std::filesystem::exists(package_path.parent_path())) {
    if (!std::filesystem::create_directories(package_path.parent_path())) {
      return X_ERROR_ACCESS_DENIED;
    }
  }

  const ContentPackage::PackageType saving_package_type =
      ContentPackage::PackageType::host;

  auto package = ContentPackage::CreatePackage(
      kernel_state_, saving_package_type, package_path, &data);

  assert_not_null(package);
  package->CreatePackageFile(flags);
  package->MountPackage(root_name);

  open_packages_.insert({string_key::create(root_name), package.release()});

  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::OpenContent(const std::string_view root_name,
                                     const XCONTENT_AGGREGATE_DATA& data,
                                     const uint32_t disc_number) {
  auto global_lock = global_critical_region_.Acquire();

  if (open_packages_.count(string_key(root_name))) {
    // Already content open with this root name.
    return X_ERROR_ALREADY_EXISTS;
  }

  auto package_path = ResolvePackagePath(data, disc_number);
  if (!std::filesystem::exists(package_path)) {
    // Does not exist, must be created.
    return X_ERROR_FILE_NOT_FOUND;
  }

  // Open package.
  auto package = ResolvePackage(data, disc_number);
  assert_not_null(package);

  package->MountPackage(root_name);
  open_packages_.insert({string_key::create(root_name), package.release()});

  return X_ERROR_SUCCESS;
}

uint32_t ContentManager::GetContentLicense(
    const std::string_view root_name) const {
  auto global_lock = global_critical_region_.Acquire();

  if (!open_packages_.count(string_key(root_name))) {
    // Already content open with this root name.
    return 0;
  }

  return open_packages_.at(string_key(root_name))->GetLicense();
}

X_RESULT ContentManager::CloseContent(const std::string_view root_name) {
  auto global_lock = global_critical_region_.Acquire();

  auto it = open_packages_.find(string_key(root_name));
  if (it == open_packages_.end()) {
    return X_ERROR_FILE_NOT_FOUND;
  }
  CloseOpenedFilesFromContent(root_name);

  auto package = it->second;
  open_packages_.erase(it);
  delete package;

  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::GetContentThumbnail(
    const XCONTENT_AGGREGATE_DATA& data, std::vector<uint8_t>* buffer) {
  auto global_lock = global_critical_region_.Acquire();
  auto package = ResolvePackage(data);
  if (!package) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  const auto thumbnail = package->GetPackageThumbnail();
  memcpy(buffer->data(), thumbnail.data(), thumbnail.size());
  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::SetContentThumbnail(
    const XCONTENT_AGGREGATE_DATA& data, std::vector<uint8_t> buffer) {
  auto global_lock = global_critical_region_.Acquire();
  auto package = ResolvePackage(data);
  if (!package) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  package->SetPackageThumbnail(buffer);

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
  auto title_id = fmt::format("{:08X}", kernel_state_->title_id());
  auto user_name =
      xe::to_path(kernel_state_->user_profile(uint32_t(0))->name());

  // Per-game per-profile data location:
  // content_root/title_id/profile/user_name
  return root_path_ / title_id / kGameUserContentDirName / user_name;
}

bool ContentManager::IsContentOpen(const XCONTENT_AGGREGATE_DATA& data) const {
  return std::any_of(open_packages_.cbegin(), open_packages_.cend(),
                     [data](std::pair<string_key, ContentPackage*> content) {
                       return &data == content.second->GetPackageContentData();
                     });
}

void ContentManager::CloseOpenedFilesFromContent(
    const std::string_view root_name) {
  // TODO(Gliniak): Cleanup this code to care only about handles
  // related to provided content
  const std::vector<object_ref<XFile>> all_files_handles =
      kernel_state_->object_table()->GetObjectsByType<XFile>(
          XObject::Type::File);

  std::string resolved_path = "";
  kernel_state_->file_system()->FindSymbolicLink(std::string(root_name) + ':',
                                                 resolved_path);

  for (const object_ref<XFile>& file : all_files_handles) {
    std::string file_path = file->entry()->absolute_path();
    bool is_file_inside_content = utf8::starts_with(file_path, resolved_path);

    if (is_file_inside_content) {
      file->ReleaseHandle();
    }
  }
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe
