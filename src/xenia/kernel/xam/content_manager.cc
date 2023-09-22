/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/content_manager.h"

#include <array>
#include <string>

#include "third_party/fmt/include/fmt/format.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/string.h"
#include "xenia/emulator.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/user_module.h"
#include "xenia/kernel/xam/user_profile.h"
#include "xenia/kernel/xfile.h"
#include "xenia/kernel/xobject.h"
#include "xenia/vfs/devices/host_path_device.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_device.h"
#include "xenia/vfs/devices/xcontent_devices/svod/svod_container_device.h"
#include "xenia/base/logging.h"

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
  auto package_root = ResolvePackageRoot(data.content_type, data.title_id);
  std::string disc_directory = "";
  std::filesystem::path package_path =
      package_root / xe::to_path(data.file_name());

  if (disc_number != -1) {
    package_path /= fmt::format("disc{:03}", disc_number);
  }
  return package_path;
}

std::vector<XCONTENT_AGGREGATE_DATA> ContentManager::ListContent(
    uint32_t device_id, XContentType content_type, uint32_t title_id) {
  std::vector<XCONTENT_AGGREGATE_DATA> result;

  if (title_id == kCurrentlyRunningTitleId) {
    title_id = kernel_state_->title_id();
  }

  // Search path:
  // content_root/title_id/type_name/*
  auto package_root = ResolvePackageRoot(content_type, title_id);
  auto file_infos = xe::filesystem::ListFiles(package_root);
  for (const auto& file_info : file_infos) {
    if (file_info.type != xe::filesystem::FileInfo::Type::kDirectory) {
      // Directories only.
      continue;
    }

    XCONTENT_AGGREGATE_DATA content_data;
    content_data.device_id = device_id;
    content_data.content_type = content_type;
    content_data.set_display_name(xe::path_to_utf16(file_info.name));
    content_data.set_file_name(xe::path_to_utf8(file_info.name));
    content_data.title_id = title_id;
    result.emplace_back(std::move(content_data));
  }
  return result;
}

bool ContentManager::ContentExists(const XCONTENT_AGGREGATE_DATA& data) {
  auto path = ResolvePackagePath(data);
  return std::filesystem::exists(path);
}

X_RESULT ContentManager::CreateContent(const std::string_view root_name,
                                       const uint32_t user_index,
                                       const XCONTENT_AGGREGATE_DATA& data) {
  auto global_lock = global_critical_region_.Acquire();

  auto package_path = ResolvePackagePath(data);
  if (std::filesystem::exists(package_path)) {
    // Exists, must not!
    return X_ERROR_ALREADY_EXISTS;
  }

  // Check if parent directories exist, if not create them
  if (!std::filesystem::exists(package_path.parent_path())) {
    if (!std::filesystem::create_directories(package_path.parent_path())) {
      return X_ERROR_ACCESS_DENIED;
    }
  }
  xex2_opt_execution_info* exec_info = nullptr;
  auto exe_module = kernel_state_->GetExecutableModule();
  if (exe_module) {
    exe_module->GetOptHeader(XEX_HEADER_EXECUTION_INFO, &exec_info);
  }
  kernel::util::XdbfBlock icon_block = {};
  const kernel::util::XdbfGameData db = kernel_state_->module_xdbf(exe_module);
  if (db.is_valid()) {
    icon_block = db.icon();
  }

  CreateContentContainer(package_path);
  WritePackageHeader(package_path, data, 0, 0, nullptr, icon_block);

  std::string device_name = std::string(root_name.data(), root_name.size());
  device_name.append(":");
  X_STATUS result = 0;
  auto device = MountPackage(package_path, result, device_name);

  if (device) {
    device->Initialize();

    //kernel_state_->file_system()->RegisterSymbolicLink("game:", mount_path);
    kernel_state_->file_system()->RegisterDevice(std::move(device));
  }
  // auto package = ResolvePackage(root_name, data);
  // assert_not_null(package);

  return result;
}

X_RESULT ContentManager::OpenContent(const std::string_view root_name,
                                     const XCONTENT_AGGREGATE_DATA& data,
                                     const uint32_t disc_number) {
  auto global_lock = global_critical_region_.Acquire();

  auto package_path = ResolvePackagePath(data, disc_number);
  if (!std::filesystem::exists(package_path)) {
    // Does not exist, must be created.
    return X_ERROR_FILE_NOT_FOUND;
  }

  X_STATUS result;
  MountPackage(package_path, result, "//Device//Content69//");
  // Open package.
  // auto package = ResolvePackage(root_name, data, disc_number);
  // assert_not_null(package);
  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::CloseContent(const std::string_view root_name) {
  auto global_lock = global_critical_region_.Acquire();
  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::GetContentThumbnail(
    const XCONTENT_AGGREGATE_DATA& data, std::vector<uint8_t>* buffer) {
  auto global_lock = global_critical_region_.Acquire();
  auto package_path = ResolvePackagePath(data);
  return X_ERROR_FILE_NOT_FOUND;
}

X_RESULT ContentManager::SetContentThumbnail(
    const XCONTENT_AGGREGATE_DATA& data, std::vector<uint8_t> buffer) {
  auto global_lock = global_critical_region_.Acquire();
  auto package_path = ResolvePackagePath(data);
  if (std::filesystem::exists(package_path)) {
    return X_ERROR_SUCCESS;
  } else {
    return X_ERROR_FILE_NOT_FOUND;
  }
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

bool ContentManager::IsContentOpen(const XCONTENT_AGGREGATE_DATA& data) const {
  return true;
}
bool ContentManager::IsValidPackage(const std::filesystem::path host_path) {
  XContentHeader* header = GetContentHeader(host_path);

  if (!header) {
    return false;
  }

  return header->is_magic_valid();
}

std::unique_ptr<vfs::Device> ContentManager::MountPackage(
    const std::filesystem::path host_path, X_RESULT& result,
    const std::string_view device_path) {
  if (!IsValidPackage(host_path)) {
    result = X_ERROR_INVALID_PARAMETER;
    return nullptr;
  }

  FILE* package_file = xe::filesystem::OpenFile(host_path, "rb");
  if (!package_file) {
    result = X_ERROR_ACCESS_DENIED;
    return nullptr;
  }

  // Read header & metadata
  XContentHeader* header = GetContentHeader(host_path);
  if (!header) {
    result = X_ERROR_ACCESS_DENIED;
    return nullptr;
  }

  XContentMetadata* metadata = GetContentMetadata(host_path);
  if (!metadata) {
    result = X_ERROR_ACCESS_DENIED;
    return nullptr;
  }

  fclose(package_file);

  switch (metadata->volume_type) {
    case XContentVolumeType::kStfs: {
      const std::string open_mode =
          metadata->volume_descriptor.stfs.flags.bits.read_only_format ? "rb"
                                                                       : "rb+";
      const uint32_t start_offset = xe::round_up(
          header->header_size, vfs ::XContentContainerDevice::kBlockSize);

      std::unique_ptr<filesystem::File> file =
          filesystem::File::Open(host_path, open_mode, start_offset);

      result = X_ERROR_SUCCESS;

      return std::make_unique<vfs::StfsContainerDevice>(
          device_path, host_path, std::move(file),
          &metadata->volume_descriptor);
    } break;
    case XContentVolumeType::kSvod: {
      const std::map<size_t, FILE*> data_files =
          GetDataFiles(host_path, metadata->data_file_count);

      result = X_ERROR_SUCCESS;

      return std::make_unique<vfs::SvodContainerDevice>(
          device_path, host_path, data_files, &metadata->volume_descriptor);
      break;
    }
    default:
      break;
  }
  return nullptr;
}

XContentHeader* ContentManager::GetContentHeader(
    const std::filesystem::path host_path) {
  XContentHeader* header = new XContentHeader();

  FILE* package_file = xe::filesystem::OpenFile(host_path, "rb");
  if (!package_file) {
    return nullptr;
  }

  const size_t processed_bytes =
      fread(header, 1, sizeof(XContentHeader), package_file);

  fclose(package_file);

  if (processed_bytes != sizeof(XContentHeader)) {
    return nullptr;
  }

  return header;
}

XContentMetadata* ContentManager::GetContentMetadata(
    const std::filesystem::path host_path) {
  XContentMetadata* metadata = new XContentMetadata();

  FILE* package_file = xe::filesystem::OpenFile(host_path, "rb");
  if (!package_file) {
    return nullptr;
  }

  xe::filesystem::Seek(package_file, sizeof(XContentHeader), 0);

  const size_t processed_bytes =
      fread(metadata, 1, sizeof(XContentMetadata), package_file);

  fclose(package_file);

  if (processed_bytes != sizeof(XContentMetadata)) {
    return nullptr;
  }
  return metadata;
}

X_RESULT ContentManager::CreateContentContainer(
    const std::filesystem::path host_path) {
  if (!xe::filesystem::CreateEmptyFile(host_path)) {
    // TODO: Add logging
    return X_ERROR_ACCESS_DENIED;
  }

  FILE* package_file = xe::filesystem::OpenFile(host_path, "wb+");
  if (!package_file) {
    return X_ERROR_ACCESS_DENIED;
  }

  // Write empty basic header
  XContentHeader* content_header = new XContentHeader();
  std::memset(content_header, 0, sizeof(XContentHeader));
  fwrite(content_header, sizeof(XContentHeader), 1, package_file);

  // Write empty content metadata
  XContentMetadata* content_metadata = new XContentMetadata();
  std::memset(content_metadata, 0, sizeof(XContentMetadata));
  fwrite(content_metadata, sizeof(XContentMetadata), 1, package_file);

  // Two elements are still missing here.
  // These are optional structures
  fclose(package_file);

  std::filesystem::resize_file(
      host_path, xe::round_up(std::filesystem::file_size(host_path),
                              vfs::XContentContainerDevice::kBlockSize));

  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::WritePackageHeader(
    const std::filesystem::path host_path, const XCONTENT_AGGREGATE_DATA& data,
    const uint64_t xuid, const uint32_t flags, XCONTENT_AGGREGATE_DATA* device,
    const util::XdbfBlock title_icon) {
  X_RESULT result = WriteContentHeader(host_path);
  if (XFAILED(result)) {
    // XELOGE()
    return result;
  }

  xex2_opt_execution_info* exec_info = nullptr;
  auto exe_module = kernel_state_->GetExecutableModule();
  if (exe_module) {
    exe_module->GetOptHeader(XEX_HEADER_EXECUTION_INFO, &exec_info);
  }

  result =
      WriteContentMetadata(host_path, data, exec_info, nullptr, title_icon);
  if (XFAILED(result)) {
    // XELOGE()
  }

  return result;
}

X_RESULT ContentManager::WriteContentHeader(
    const std::filesystem::path host_path) {
  XContentHeader* header = new XContentHeader();

  std::memset(header, 0, sizeof(XContentHeader));
  // We can hardcode it to kCon because it is only type that can be created by
  // console.
  header->magic = XContentPackageType::kCon;
  header->signature.console.console_certificate.console_type =
      XConsoleType::Retail;
  header->header_size = sizeof(XContentHeader) + sizeof(XContentMetadata);

  // Do not fill console_id right now, however it will be useful in near future.
  header->signature.console.console_certificate.console_id[0] = 0;

  // Open host file and write header to it. It is start of file so we can ignore
  // offset.
  FILE* package_file = xe::filesystem::OpenFile(host_path, "rb+");
  if (!package_file) {
    return X_ERROR_ACCESS_DENIED;
  }

  // We're opening empty file in append mode.
  // We must set position to start of file.
  xe::filesystem::Seek(package_file, 0, 0);

  // Write empty basic header
  fwrite(header, sizeof(XContentHeader), 1, package_file);
  fclose(package_file);
  return X_ERROR_SUCCESS;
}

X_RESULT ContentManager::WriteContentMetadata(
    const std::filesystem::path host_path, const XCONTENT_AGGREGATE_DATA& data,
    const xex2_opt_execution_info* execution_info,
    const vfs::StfsVolumeDescriptor* descriptor,
    const util::XdbfBlock title_icon) {
  FILE* package_file = xe::filesystem::OpenFile(host_path, "rb+");
  if (!package_file) {
    return X_ERROR_ACCESS_DENIED;
  }

  if (!xe::filesystem::Seek(package_file, sizeof(XContentHeader), 0)) {
    // TODO: Add error message
    return X_ERROR_ACCESS_DENIED;
  }

  // Preparing metadata
  XContentMetadata* metadata = new XContentMetadata();

  std::memset(metadata, 0, sizeof(XContentMetadata));

  metadata->content_type = data.content_type;
  metadata->metadata_version = 2;
  metadata->content_size = 0;

  std::memcpy(&metadata->execution_info, execution_info,
              sizeof(xex2_opt_execution_info));

  // TODO: Allow writing custom console_id
  // metadata->console_id[0] = 0;

  metadata->profile_id = kernel_state_->user_profile(uint32_t(0))->xuid();

  // Initially volume descriptor is not filled at all!
  if (descriptor) {
    std::memcpy(&metadata->volume_descriptor, descriptor,
                sizeof(vfs::StfsVolumeDescriptor));
  }

  metadata->volume_type = XContentVolumeType::kStfs;

  // TODO: Allow writing device_id.
  // metadata->device_id[0] = 0;

  if (title_icon) {
    if (title_icon.size <= XContentMetadata::kThumbLengthV2) {
      std::copy_n(title_icon.buffer, title_icon.size,
                  metadata->title_thumbnail);

      metadata->title_thumbnail_size = (uint32_t)title_icon.size;

      std::copy_n(title_icon.buffer, title_icon.size, metadata->thumbnail);

      metadata->thumbnail_size = (uint32_t)title_icon.size;
    }
  }

  // Write new metadata
  fwrite(metadata, sizeof(XContentMetadata), 1, package_file);
  fclose(package_file);
  return X_ERROR_SUCCESS;
}

std::map<size_t, FILE*> ContentManager::GetDataFiles(
    const std::filesystem::path host_path, const uint32_t file_count) {
  std::map<size_t, FILE*> files = {};

  std::filesystem::path data_path = host_path;
  data_path += ".data";

  if (!std::filesystem::exists(data_path)) {
    XELOGE("STFS container is multi-file, but path {} does not exist.",
            xe::path_to_utf8(data_path));
    return files;
  }

  auto fragment_files = filesystem::ListFiles(data_path);
  std::sort(fragment_files.begin(), fragment_files.end(),
            [](filesystem::FileInfo& left, filesystem::FileInfo& right) {
              return left.name < right.name;
            });

  if (fragment_files.size() != file_count) {
    XELOGE("SVOD expecting {} data fragments, but {} are present.", file_count,
           fragment_files.size());
    return files;
  }

  for (size_t i = 0; i < fragment_files.size(); i++) {
    auto& fragment = fragment_files.at(i);
    auto path = fragment.path / fragment.name;
    auto file = xe::filesystem::OpenFile(path, "rb");
    if (!file) {
      XELOGI("Failed to map SVOD file {}.", xe::path_to_utf8(path));
      //CloseFiles();
      return files;
    }

    // no need to seek back, any reads from this file will seek first anyway
    files.emplace(std::make_pair(i, file));
  }
  XELOGI("SVOD successfully mapped {} files.", fragment_files.size());
  return files;
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe
