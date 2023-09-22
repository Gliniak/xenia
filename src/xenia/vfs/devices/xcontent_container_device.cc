/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/logging.h"
#include "xenia/vfs/devices/xcontent_container_device.h"

namespace xe {
namespace vfs {

XContentContainerDevice::XContentContainerDevice(
    const std::string_view mount_path, const std::filesystem::path& host_path,
    VolumeDescriptor* volume_descriptor)
    : Device(mount_path),
      name_("XContent"),
      host_path_(host_path),
      volume_descriptor_(volume_descriptor) {
}

XContentContainerDevice::~XContentContainerDevice() {}

bool XContentContainerDevice::Initialize() {
  return Read() == Result::kSuccess;
}

Entry* XContentContainerDevice::ResolvePath(const std::string_view path) {
  // The filesystem will have stripped our prefix off already, so the path will
  // be in the form:
  // some\PATH.foo
  XELOGFS("StfsContainerDevice::ResolvePath({})", path);
  return root_entry_->ResolvePath(path);
}

void XContentContainerDevice::Dump(StringBuffer* string_buffer) {
  auto global_lock = global_critical_region_.Acquire();
  root_entry_->Dump(string_buffer, 0);
}

}  // namespace vfs
}  // namespace xe