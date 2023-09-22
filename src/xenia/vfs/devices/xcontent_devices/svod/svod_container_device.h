/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_XCONTENT_DEVICES_SVOD_CONTAINER_DEVICE_H_
#define XENIA_VFS_DEVICES_XCONTENT_DEVICES_SVOD_CONTAINER_DEVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "xenia/base/string_util.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/vfs/device.h"
#include "xenia/vfs/devices/stfs_xbox.h"
#include "xenia/vfs/devices/xcontent_container_device.h"
#include "xenia/vfs/devices/xcontent_container_entry.h"

namespace xe {
namespace vfs {

class SvodContainerDevice : public XContentContainerDevice {
 public:
  SvodContainerDevice(const std::string_view mount_path,
                      const std::filesystem::path& host_path,
                      const std::map<size_t, FILE*> data_files,
                      VolumeDescriptor* volume_descriptor);
  ~SvodContainerDevice() override;

  bool is_read_only() const override { return true; }

  uint32_t total_allocation_units() const override {
    return uint32_t(files_total_size_ / sectors_per_allocation_unit() /
                    bytes_per_sector());
  }

  void CloseFiles(){};

 private:
  enum class SvodLayoutType {
    kUnknown = 0x0,
    kEnhancedGDF = 0x1,
    kXSF = 0x2,
    kSingleFile = 0x4,
  };
  const char* MEDIA_MAGIC = "MICROSOFT*XBOX*MEDIA";

  Result Read() override;
  Result ReadEntry(uint32_t sector, uint32_t ordinal,
                  XContentContainerEntry* parent);
  void BlockToOffset(size_t sector, size_t* address, size_t* file_index) const;

  Result SetLayout(FILE* header, size_t& magic_offset);
  Result SetEDGFLayout(FILE* header, size_t& magic_offset);
  Result SetXSFLayout(FILE* header, size_t& magic_offset);
  Result SetNormalLayout(FILE* header, size_t& magic_offset);

  const bool IsEDGFLayout() const {
    return volume_descriptor_->svod.features.bits.enhanced_gdf_layout;
  }
  const bool IsXSFLayout(FILE* header) const;

  std::map<size_t, FILE*> files_;
  size_t files_total_size_;

  size_t svod_base_offset_;
  SvodLayoutType svod_layout_;

};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_XCONTENT_DEVICES_SVOD_CONTAINER_DEVICE_H_
