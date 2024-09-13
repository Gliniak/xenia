/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_CONTENT_XCONTENT_PACKAGE_H_
#define XENIA_KERNEL_XAM_CONTENT_XCONTENT_PACKAGE_H_

#include <string>
#include "xenia/base/mapped_memory.h"
#include "xenia/kernel/xam/content/content_package.h"
#include "xenia/kernel/xam/content/xcontent.h"

namespace xe {
namespace kernel {
namespace xam {

class XContentPackage : public ContentPackage {
 public:
  XContentPackage(KernelState* kernel_state,
                  const std::filesystem::path& package_path,
                  const XCONTENT_AGGREGATE_DATA* data = nullptr);
  ~XContentPackage(){};

  bool CreatePackageFile(const uint32_t flags);
  bool MountPackage(const std::string_view root_name) override;
  std::vector<uint8_t> GetPackageThumbnail() const override;
  void SetPackageThumbnail(const std::vector<uint8_t> buffer) override;
  uint32_t GetLicense() const override;

 private:
  bool LoadAndReadHeader(bool writable = false);
  bool WriteBasicHeaderInfo(const uint32_t flags);
  bool IsPackageValid();
  bool Initialize() override;

  std::unique_ptr<XContentContainerHeader> header_;
  std::unique_ptr<MappedMemory> mapped_header_;
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif