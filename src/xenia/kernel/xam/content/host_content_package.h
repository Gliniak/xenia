/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_CONTENT_HOST_CONTENT_PACKAGE_H_
#define XENIA_KERNEL_XAM_CONTENT_HOST_CONTENT_PACKAGE_H_

#include <string>
#include "xenia/kernel/xam/content/content_package.h"
#include "xenia/kernel/xam/content/xcontent.h"

namespace xe {
namespace kernel {
class KernelState;
}  // namespace kernel
}  // namespace xe

namespace xe {
namespace kernel {
namespace xam {

static const char* kThumbnailFileName = "__thumbnail.png";
static const char* kGameContentHeaderDirName = "Headers";

class HostContentPackage : public ContentPackage {
 public:
  HostContentPackage(KernelState* kernel_state,
                     const std::filesystem::path& package_path,
                     const XCONTENT_AGGREGATE_DATA* data = nullptr);

  HostContentPackage(KernelState* kernel_state,
                     const std::filesystem::path& package_path,
                     const uint32_t device_id, const uint32_t title_id,
                     const XContentType content_type);
  ~HostContentPackage(){};

  bool CreatePackageFile(const uint32_t flags) override;
  bool MountPackage(const std::string_view root_name) override;

  std::vector<uint8_t> GetPackageThumbnail() const override;
  void SetPackageThumbnail(const std::vector<uint8_t> buffer) override;
  uint32_t GetLicense() const override;

 private:
  bool Initialize() override;
  X_RESULT ReadHeader(const uint32_t device_id, const uint32_t title_id,
                      const XContentType content_type);
  bool WriteHeader();
  std::filesystem::path ResolveHeaderPath(const uint32_t title_id,
                                          const XContentType content_type);
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif