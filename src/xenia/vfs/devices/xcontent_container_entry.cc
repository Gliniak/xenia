/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/xcontent_container_entry.h"
#include "xenia/vfs/devices/xcontent_container_file.h"

#include <map>

namespace xe {
namespace vfs {

XContentContainerEntry::XContentContainerEntry(Device* device, Entry* parent,
                                               const std::string_view path)
    : Entry(device, parent, path),
      data_offset_(0),
      data_size_(0),
      block_(0) {}

XContentContainerEntry::~XContentContainerEntry() = default;

std::unique_ptr<XContentContainerEntry> XContentContainerEntry::Create(
    Device* device, Entry* parent, const std::string_view name) {
  return nullptr;
}

X_STATUS XContentContainerEntry::Open(uint32_t desired_access,
                                      File** out_file) {
  return X_ERROR_FUNCTION_FAILED;
}

}  // namespace vfs
}  // namespace xe