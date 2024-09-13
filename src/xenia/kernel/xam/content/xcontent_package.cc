/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/content/xcontent_package.h"
#include "third_party/fmt/include/fmt/format.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/user_module.h"
#include "xenia/kernel/xam/content/content_manager.h"
#include "xenia/kernel/xam/content/content_package.h"
#include "xenia/kernel/xam/content/xcontent.h"
#include "xenia/kernel/xam/user_profile.h"
#include "xenia/vfs/devices/xcontent_container_device.h"

namespace xe {
namespace kernel {
namespace xam {

XContentPackage::XContentPackage(KernelState* kernel_state,
                                 const std::filesystem::path& package_path,
                                 const XCONTENT_AGGREGATE_DATA* data)
    : ContentPackage(kernel_state, package_path, data){};

bool XContentPackage::MountPackage(const std::string_view root_name) {
  SetRootName(root_name);
  const std::string_view device_path_ = GetDevicePath();
  auto fs = kernel_state_->file_system();
  auto device = vfs::XContentContainerDevice::CreateContentDevice(
      device_path_, GetPackagePath());
  device->Initialize();
  fs->RegisterDevice(std::move(device));
  fs->RegisterSymbolicLink(std::string(GetRootName()) + ":", device_path_);

  return true;
}

bool XContentPackage::CreatePackageFile(const uint32_t flags) {
  if (std::filesystem::exists(GetPackagePath())) {
    return false;
  }

  if (!xe::filesystem::CreateEmptyFile(GetPackagePath())) {
    return false;
  }

  std::error_code resize_error;
  std::filesystem::resize_file(
      GetPackagePath(), xe::round_up(sizeof(XContentContainerHeader), 0x1000),
      resize_error);

  if (resize_error) {
    return false;
  }

  if (!LoadAndReadHeader(true)) {
    return false;
  }

  WriteBasicHeaderInfo(flags);

  memcpy(mapped_header_->data(), header_.get(),
         sizeof(XContentContainerHeader));
  mapped_header_->Flush();

  return true;
}
bool XContentPackage::WriteBasicHeaderInfo(const uint32_t flags) {
  header_->content_header.magic = XContentPackageType::kCon;
  header_->content_header.licenses[0].licensee_id = static_cast<uint64_t>(-1);
  header_->content_header.header_size = sizeof(XContentContainerHeader);

  if (content_data_) {
    header_->content_metadata.content_type = content_data_->content_type;
    header_->content_metadata.metadata_version = 2;

    xex2_opt_execution_info* opt_exec_info = nullptr;

    kernel_state_->GetExecutableModule()->GetOptHeader(
        XEX_HEADER_EXECUTION_INFO, &opt_exec_info);

    memcpy(&header_->content_metadata.execution_info, opt_exec_info,
           sizeof(xex2_opt_execution_info));

    std::string console_id = "XENIA";
    memcpy(header_->content_metadata.console_id, console_id.c_str(), 5);

    header_->content_metadata.profile_id =
        kernel_state_->user_profile(static_cast<uint32_t>(0))->xuid();

    // TODO!
    header_->content_metadata.device_id[0] = 0;

    memcpy(header_->content_metadata.display_name_raw.chars,
           content_data_->display_name_raw.chars, 128);

    std::u16string title_name =
        xe::to_utf16(kernel_state_->title_xdbf().title());

    xe::string_util::copy_and_swap_truncating(
        header_->content_metadata.title_name_raw.chars, title_name, 64);

    header_->content_metadata.flags.as_byte = flags;

    const auto icon = kernel_state_->title_xdbf().icon();
    if (icon) {
      header_->content_metadata.thumbnail_size =
          static_cast<uint32_t>(icon.size);
      header_->content_metadata.title_thumbnail_size =
          static_cast<uint32_t>(icon.size);

      memcpy(header_->content_metadata.thumbnail, icon.buffer, icon.size);
      memcpy(header_->content_metadata.title_thumbnail, icon.buffer, icon.size);
    }
  }

  header_->content_header.signature.console.console_certificate.console_type =
      XConsoleType::Retail;
  return true;
}

bool XContentPackage::LoadAndReadHeader(bool writable) {
  mapped_header_ =
      MappedMemory::Open(GetPackagePath(),
                         !writable ? xe::MappedMemory::Mode::kRead
                                   : xe::MappedMemory::Mode::kReadWrite,
                         0, sizeof(XContentContainerHeader));

  if (!mapped_header_) {
    return false;
  }

  header_ = std::make_unique<XContentContainerHeader>();
  memcpy(header_.get(), mapped_header_->data(),
         sizeof(XContentContainerHeader));

  mapped_header_->Close();
  return true;
}

bool XContentPackage::Initialize() {
  if (!LoadAndReadHeader()) {
    return false;
  }

  if (!IsPackageValid()) {
    return false;
  }

  content_data_->content_type = header_->content_metadata.content_type;
  content_data_->device_id = 0;

  memcpy(content_data_->display_name_raw.uint,
         header_->content_metadata.display_name_raw.uint, 128);

  content_data_->title_id = header_->content_metadata.execution_info.title_id;
  content_data_->xuid = header_->content_metadata.profile_id;
  memset(content_data_->file_name_raw, 0, 42);

  const auto filename = xe::path_to_utf8(GetPackagePath().filename());

  memcpy(content_data_->file_name_raw, filename.c_str(), filename.length());

  return true;
}

bool XContentPackage::IsPackageValid() {
  if (!header_) {
    return false;
  }

  if (!header_->content_header.is_magic_valid()) {
    return false;
  }

  return true;
}

std::vector<uint8_t> XContentPackage::GetPackageThumbnail() const {
  std::vector<uint8_t> title_thumbnail;
  title_thumbnail.resize(header_->content_metadata.thumbnail_size);

  memcpy(title_thumbnail.data(), header_->content_metadata.thumbnail,
         header_->content_metadata.thumbnail_size);

  return title_thumbnail;
}

void XContentPackage::SetPackageThumbnail(const std::vector<uint8_t> buffer) {
  const size_t size =
      std::min(buffer.size(), sizeof(header_->content_metadata.thumbnail));

  memcpy(header_->content_metadata.thumbnail, buffer.data(), size);

  header_->content_metadata.thumbnail_size = static_cast<uint32_t>(size);
}

uint32_t XContentPackage::GetLicense() const {
  return header_->content_header.licenses[0].license_bits;
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe