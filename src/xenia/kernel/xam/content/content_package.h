/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_CONTENT_CONTENT_PACKAGE_H_
#define XENIA_KERNEL_XAM_CONTENT_CONTENT_PACKAGE_H_

#include <string>
#include "xenia/kernel/xam/content/xcontent.h"

namespace xe {
namespace kernel {
class KernelState;
}  // namespace kernel
}  // namespace xe

namespace xe {
namespace kernel {
namespace xam {

class ContentPackage {
 public:
  enum class PackageType { host = 0, xcontent = 1, unknown = 0xFF };

  ContentPackage(KernelState* kernel_state,
                 const std::filesystem::path& package_path,
                 const XCONTENT_AGGREGATE_DATA* data = nullptr);
  ~ContentPackage();

  static std::unique_ptr<ContentPackage> CreatePackage(
      KernelState* kernel_state, const PackageType packageType,
      const std::filesystem::path& package_path,
      const XCONTENT_AGGREGATE_DATA* data = nullptr);

  const XCONTENT_AGGREGATE_DATA* GetPackageContentData() const {
    return content_data_.get();
  }

  virtual bool CreatePackageFile(const uint32_t flags) = 0;
  virtual bool MountPackage(const std::string_view root_name) = 0;

  virtual void SetPackageThumbnail(const std::vector<uint8_t> buffer) = 0;
  virtual std::vector<uint8_t> GetPackageThumbnail() const = 0;
  virtual uint32_t GetLicense() const = 0;

 protected:
  std::string_view GetDevicePath() const { return device_path_; }
  std::string_view GetRootName() const { return root_name_; }
  std::filesystem::path GetPackagePath() const { return package_path_; }

  void SetRootName(const std::string_view root_name) {
    if (!root_name_.empty()) {
      return;
    }
    root_name_ = root_name;
  }

  KernelState* kernel_state_;
  std::unique_ptr<XCONTENT_AGGREGATE_DATA> content_data_;

 private:
  virtual bool Initialize() = 0;

  std::string device_path_;
  std::string root_name_ = "";
  std::filesystem::path package_path_;
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_CONTENT_CONTENT_PACKAGE_H_