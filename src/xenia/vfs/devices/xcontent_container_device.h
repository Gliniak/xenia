/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_XCONTENT_CONTAINER_DEVICE_H_
#define XENIA_VFS_DEVICES_XCONTENT_CONTAINER_DEVICE_H_

#include <filesystem>
#include <map>
#include <string_view>

#include "xenia/base/math.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/vfs/device.h"
#include "xenia/vfs/devices/stfs_xbox.h"
#include "xenia/kernel/xam/content_manager.h"

#include "xenia/vfs/devices/xcontent_container_entry.h"

namespace xe {
namespace vfs {
class XContentContainerDevice : public Device {
 public:
  const static uint32_t kBlockSize = 0x1000;
  XContentContainerDevice(const std::string_view mount_path,
                          const std::filesystem::path& host_path,
                          VolumeDescriptor* volume_descriptor);
  ~XContentContainerDevice();

  bool Initialize();

  virtual bool is_read_only() const = 0;

  const std::string& name() const override { return name_; }

  uint32_t attributes() const override { return 0; }
  uint32_t sectors_per_allocation_unit() const override { return 8; }
  uint32_t bytes_per_sector() const override { return 0x200; }
  uint32_t available_allocation_units() const override { return 0; }
  uint32_t component_name_max_length() const override { return 255; }
 protected:

  enum class Result {
    kSuccess = 0,
    kOutOfMemory = -1,
    kReadError = -10,
    kFileMismatch = -30,
    kDamagedFile = -31,
    kTooSmall = -32,
  };

  virtual Result Read() = 0;
  virtual void CloseFiles() = 0;

  Entry* ResolvePath(const std::string_view path);

  void SetName(std::string name) { name_ = name; }
  const std::string& GetName() const { return name_; }

  const std::filesystem::path& GetHostPath() const { return host_path_; }

  void Dump(StringBuffer* string_buffer);

  std::string name_;
  std::filesystem::path host_path_;

  std::unique_ptr<XContentContainerEntry> root_entry_;

  VolumeDescriptor* volume_descriptor_;

};

}  // namespace vfs
}  // namespace xe

#endif
