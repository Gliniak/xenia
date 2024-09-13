/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_CONTENT_MANAGER_H_
#define XENIA_KERNEL_XAM_CONTENT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "xenia/base/memory.h"
#include "xenia/base/mutex.h"
#include "xenia/base/string_key.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/xam/content/content_package.h"
#include "xenia/kernel/xam/content/xcontent.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
class KernelState;
}  // namespace kernel
}  // namespace xe

namespace xe {
namespace kernel {
namespace xam {

class ContentManager {
 public:
  ContentManager(KernelState* kernel_state,
                 const std::filesystem::path& root_path);
  ~ContentManager();

  std::filesystem::path GetRootPath() const { return root_path_; }

  std::vector<XCONTENT_AGGREGATE_DATA> ListContent(
      const uint32_t device_id, const XContentType content_type,
      uint32_t title_id = -1);

  std::unique_ptr<ContentPackage> ResolvePackage(
      const XCONTENT_AGGREGATE_DATA& data, const uint32_t disc_number = -1);

  bool ContentExists(const XCONTENT_AGGREGATE_DATA& data);
  X_RESULT CreateContent(const std::string_view root_name,
                         const XCONTENT_AGGREGATE_DATA& data,
                         const uint32_t flags);
  X_RESULT OpenContent(const std::string_view root_name,
                       const XCONTENT_AGGREGATE_DATA& data,
                       const uint32_t disc_number = -1);
  X_RESULT CloseContent(const std::string_view root_name);

  uint32_t GetContentLicense(const std::string_view root_name) const;

  X_RESULT GetContentThumbnail(const XCONTENT_AGGREGATE_DATA& data,
                               std::vector<uint8_t>* buffer);
  X_RESULT SetContentThumbnail(const XCONTENT_AGGREGATE_DATA& data,
                               std::vector<uint8_t> buffer);
  X_RESULT DeleteContent(const XCONTENT_AGGREGATE_DATA& data);
  std::filesystem::path ResolveGameUserContentPath();
  bool IsContentOpen(const XCONTENT_AGGREGATE_DATA& data) const;
  void CloseOpenedFilesFromContent(const std::string_view root_name);

  static std::string GenerateUniquePackageId(const std::filesystem::path path);

 private:
  std::filesystem::path ResolvePackageRoot(XContentType content_type,
                                           uint32_t title_id = -1);
  std::filesystem::path ResolvePackagePath(const XCONTENT_AGGREGATE_DATA& data,
                                           const uint32_t disc_number = -1);

  ContentPackage::PackageType ResolvePackageType(
      const std::filesystem::path path) const;

  std::unordered_set<uint32_t> FindPublisherTitleIds(
      uint32_t base_title_id = kCurrentlyRunningTitleId) const;

  KernelState* kernel_state_;
  std::filesystem::path root_path_;

  // TODO(benvanik): remove use of global lock, it's bad here!
  xe::global_critical_region global_critical_region_;
  std::unordered_map<string_key, ContentPackage*> open_packages_;
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_CONTENT_MANAGER_H_
