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
#include <vector>

#include "xenia/base/memory.h"
#include "xenia/base/mutex.h"
#include "xenia/base/string_key.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/util/xdbf_utils.h"
#include "xenia/kernel/xam/xcontent.h"
#include "xenia/xbox.h"

#include "xenia/vfs/devices/xcontent_container_device.h"
#include "xenia/vfs/device.h"

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

  std::vector<XCONTENT_AGGREGATE_DATA> ListContent(uint32_t device_id,
                                                   XContentType content_type,
                                                   uint32_t title_id = -1);

  //std::unique_ptr<ContentPackage> ResolvePackage(
  //    const std::string_view root_name, const XCONTENT_AGGREGATE_DATA& data,
   //   const uint32_t disc_number = -1);

  bool ContentExists(const XCONTENT_AGGREGATE_DATA& data);
  X_RESULT CreateContent(const std::string_view root_name,
                         const uint32_t user_index,
                         const XCONTENT_AGGREGATE_DATA& data);
  X_RESULT OpenContent(const std::string_view root_name,
                       const XCONTENT_AGGREGATE_DATA& data,
                       const uint32_t disc_number = -1);
  X_RESULT CloseContent(const std::string_view root_name);
  X_RESULT GetContentThumbnail(const XCONTENT_AGGREGATE_DATA& data,
                               std::vector<uint8_t>* buffer);
  X_RESULT SetContentThumbnail(const XCONTENT_AGGREGATE_DATA& data,
                               std::vector<uint8_t> buffer);
  X_RESULT DeleteContent(const XCONTENT_AGGREGATE_DATA& data);
  bool IsContentOpen(const XCONTENT_AGGREGATE_DATA& data) const;

  std::unique_ptr<vfs::Device> MountPackage(
      const std::filesystem::path host_path, X_RESULT& result,
      const std::string_view device_path = "");
 private:
  std::filesystem::path ResolvePackageRoot(XContentType content_type,
                                           uint32_t title_id = -1);
  std::filesystem::path ResolvePackagePath(const XCONTENT_AGGREGATE_DATA& data,
                                           const uint32_t disc_number = -1);

  std::string GenerateDeviceNameFromPath(
      const std::filesystem::path host_path) {

    const std::string guest_device_path = "//Device//Package_{:016X}";
    return "";
  };

  XContentHeader* GetContentHeader(const std::filesystem::path host_path);
  XContentMetadata* GetContentMetadata(const std::filesystem::path host_path);


  bool IsValidPackage(const std::filesystem::path host_path);


  X_RESULT CreateContentContainer(const std::filesystem::path host_path);
  X_RESULT WritePackageHeader(
      const std::filesystem::path host_path,
      const XCONTENT_AGGREGATE_DATA& data, const uint64_t xuid,
      const uint32_t flags, XCONTENT_AGGREGATE_DATA* device = nullptr,
      const util::XdbfBlock title_icon = util::XdbfBlock());

  X_RESULT WriteContentHeader(const std::filesystem::path host_path);
  X_RESULT WriteContentMetadata(const std::filesystem::path host_path,
                                const XCONTENT_AGGREGATE_DATA& data,
                                const xex2_opt_execution_info* execution_info,
                                const vfs::StfsVolumeDescriptor* descriptor = nullptr,
                                const util::XdbfBlock title_icon = util::XdbfBlock());

  std::map<size_t, FILE*> GetDataFiles(const std::filesystem::path host_path,
                                       const uint32_t file_count);

  KernelState* kernel_state_;
  std::filesystem::path root_path_;

  // TODO(benvanik): remove use of global lock, it's bad here!
  xe::global_critical_region global_critical_region_;
};

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_CONTENT_MANAGER_H_
