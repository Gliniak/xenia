/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/content/content_package.h"
#include "third_party/fmt/include/fmt/format.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/xam/content/content_manager.h"
#include "xenia/kernel/xam/content/host_content_package.h"
#include "xenia/kernel/xam/content/xcontent_package.h"
#include "xenia/vfs/devices/host_path_device.h"
#include "xenia/vfs/devices/xcontent_container_device.h"

namespace xe {
namespace kernel {
namespace xam {

std::unique_ptr<ContentPackage> ContentPackage::CreatePackage(
    KernelState* kernel_state, const PackageType packageType,
    const std::filesystem::path& package_path,
    const XCONTENT_AGGREGATE_DATA* data) {
  switch (packageType) {
    case PackageType::host:
      return std::make_unique<HostContentPackage>(kernel_state, package_path,
                                                  data);

    case PackageType::xcontent:
      return std::make_unique<XContentPackage>(kernel_state, package_path,
                                               data);

    default:
      break;
  }

  return nullptr;
}

ContentPackage::ContentPackage(KernelState* kernel_state,
                               const std::filesystem::path& package_path,
                               const XCONTENT_AGGREGATE_DATA* data)
    : kernel_state_(kernel_state),
      package_path_(package_path),
      content_data_(nullptr) {
  device_path_ =
      fmt::format("\\Device\\Package_{}",
                  ContentManager::GenerateUniquePackageId(package_path));

  if (data) {
    content_data_ = std::make_unique<XCONTENT_AGGREGATE_DATA>(*data);
  }
}

ContentPackage::~ContentPackage() {
  auto fs = kernel_state_->file_system();
  fs->UnregisterSymbolicLink(root_name_ + ":");
  fs->UnregisterDevice(device_path_);
}

}  // namespace xam
}  // namespace kernel
}  // namespace xe