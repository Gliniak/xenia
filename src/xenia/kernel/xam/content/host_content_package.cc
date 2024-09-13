/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/content/host_content_package.h"
#include "third_party/fmt/include/fmt/format.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/xam/content/content_manager.h"
#include "xenia/kernel/xam/content/content_package.h"
#include "xenia/vfs/devices/host_path_device.h"

DECLARE_int32(license_mask);

namespace xe {
namespace kernel {
namespace xam {

HostContentPackage::HostContentPackage(
    KernelState* kernel_state, const std::filesystem::path& package_path,
    const XCONTENT_AGGREGATE_DATA* data)
    : ContentPackage(kernel_state, package_path, data) {
  Initialize();
}

HostContentPackage::HostContentPackage(
    KernelState* kernel_state, const std::filesystem::path& package_path,
    const uint32_t device_id, const uint32_t title_id,
    const XContentType content_type)
    : ContentPackage(kernel_state, package_path, nullptr) {
  ReadHeader(device_id, title_id, content_type);
}

bool HostContentPackage::Initialize() {
  // content_data_.

  return true;
}

bool HostContentPackage::CreatePackageFile(const uint32_t flags) {
  if (!std::filesystem::create_directories(GetPackagePath())) {
    return false;
  }

  WriteHeader();

  return true;
}

bool HostContentPackage::MountPackage(const std::string_view root_name) {
  SetRootName(root_name);
  const std::string_view device_path_ = GetDevicePath();
  auto fs = kernel_state_->file_system();
  auto device = std::make_unique<vfs::HostPathDevice>(device_path_,
                                                      GetPackagePath(), false);
  device->Initialize();
  fs->RegisterDevice(std::move(device));
  fs->RegisterSymbolicLink(std::string(GetRootName()) + ":", device_path_);

  return true;
}

X_RESULT HostContentPackage::ReadHeader(const uint32_t device_id,
                                        const uint32_t title_id,
                                        const XContentType content_type) {
  const auto header_file_path = ResolveHeaderPath(title_id, content_type);

  if (!std::filesystem::exists(header_file_path)) {
    // Header file doesn't exist. Let's create default one.

    XCONTENT_AGGREGATE_DATA content_data;

    content_data.device_id = device_id;
    content_data.content_type = static_cast<XContentType>(content_type);
    content_data.set_display_name(
        xe::path_to_utf16(GetPackagePath().filename()));
    content_data.set_file_name(xe::path_to_utf8(GetPackagePath().filename()));
    content_data.title_id = title_id;
    content_data.xuid =
        kernel_state_->user_profile(static_cast<uint32_t>(0))->xuid();

    content_data_ = std::make_unique<XCONTENT_AGGREGATE_DATA>(content_data);
    return X_STATUS_SUCCESS;
  }

  auto file_size = std::filesystem::file_size(header_file_path);
  if (file_size != sizeof(XCONTENT_DATA) &&
      file_size != sizeof(XCONTENT_AGGREGATE_DATA)) {
    return X_STATUS_UNSUCCESSFUL;
  }

  auto file = xe::filesystem::OpenFile(header_file_path, "rb");
  if (!file) {
    return X_STATUS_UNSUCCESSFUL;
  }

  content_data_ = std::make_unique<XCONTENT_AGGREGATE_DATA>();
  size_t result = fread(content_data_.get(), 1, file_size, file);
  if (result != sizeof(XCONTENT_DATA) &&
      result != sizeof(XCONTENT_AGGREGATE_DATA)) {
    content_data_.reset();
    return X_STATUS_UNSUCCESSFUL;
  }
  fclose(file);

  content_data_->title_id = title_id;
  content_data_->xuid =
      kernel_state_->user_profile(static_cast<uint32_t>(0))->xuid();
  return X_STATUS_SUCCESS;
}

bool HostContentPackage::WriteHeader() {
  const auto header_file_path =
      ResolveHeaderPath(kernel_state_->title_id(), content_data_->content_type);
  auto parent_path = header_file_path.parent_path();

  if (!std::filesystem::exists(parent_path)) {
    if (!std::filesystem::create_directories(parent_path)) {
      return false;
    }
  }

  xe::filesystem::CreateEmptyFile(header_file_path);

  if (!std::filesystem::exists(header_file_path)) {
    return false;
  }

  auto file = xe::filesystem::OpenFile(header_file_path, "wb");
  fwrite(content_data_.get(), 1, sizeof(XCONTENT_AGGREGATE_DATA), file);
  fclose(file);
  return true;
}

std::filesystem::path HostContentPackage::ResolveHeaderPath(
    const uint32_t title_id, const XContentType content_type) {
  auto title_id_str = fmt::format("{:08X}", title_id);
  auto content_type_str =
      fmt::format("{:08X}", static_cast<uint32_t>(content_type));
  std::string final_name =
      xe::path_to_utf8(GetPackagePath().filename()) + ".header";

  // Header root path:
  // content_root/title_id/Headers/content_type/
  return kernel_state_->content_manager()->GetRootPath() / title_id_str /
         kGameContentHeaderDirName / content_type_str / final_name;
}

std::vector<uint8_t> HostContentPackage::GetPackageThumbnail() const {
  const auto thumbnail_path =
      GetPackagePath().parent_path() / kThumbnailFileName;

  if (!std::filesystem::exists(thumbnail_path)) {
    return {};
  }

  std::vector<uint8_t> buffer;
  auto file = xe::filesystem::OpenFile(thumbnail_path, "rb");
  size_t file_len = std::filesystem::file_size(thumbnail_path);
  buffer.resize(file_len);
  fread(buffer.data(), 1, buffer.size(), file);
  fclose(file);
  return buffer;
}

void HostContentPackage::SetPackageThumbnail(
    const std::vector<uint8_t> buffer) {
  std::filesystem::create_directories(GetPackagePath());

  if (std::filesystem::exists(GetPackagePath())) {
    auto thumb_path = GetPackagePath() / kThumbnailFileName;
    auto file = xe::filesystem::OpenFile(thumb_path, "wb");
    fwrite(buffer.data(), 1, buffer.size(), file);
    fclose(file);
  }
  return;
}

uint32_t HostContentPackage::GetLicense() const { return cvars::license_mask; }

}  // namespace xam
}  // namespace kernel
}  // namespace xe