/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <algorithm>
#include <cmath>

#include "xenia/base/math.h"
#include "xenia/vfs/devices/xcontent_container_entry.h"
#include "xenia/vfs/devices/xcontent_container_file.h"

namespace xe {
namespace vfs {

XContentContainerFile::XContentContainerFile(uint32_t file_access,
                                     XContentContainerEntry* entry)
    : File(file_access, entry), entry_(entry) {}

XContentContainerFile::~XContentContainerFile() = default;

void XContentContainerFile::Destroy() { delete this; }

X_STATUS XContentContainerFile::ReadSync(
    void* buffer, size_t buffer_length,
    size_t byte_offset,
    size_t* out_bytes_read) {
  return X_STATUS_END_OF_FILE;
}

}  // namespace vfs
}  // namespace xe