/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/xcontent_devices/svod/svod_container_entry.h"
#include "xenia/vfs/devices/xcontent_devices/svod/svod_container_file.h"

#include "xenia/base/clock.h"
#include "xenia/base/math.h"

#include <map>

namespace xe {
namespace vfs {

SvodContainerEntry::SvodContainerEntry(Device* device, Entry* parent,
                                       const std::string_view path,
                                       MultiFileHandles* files)
    : XContentContainerEntry(device, parent, path), files_(files) {
}

SvodContainerEntry::~SvodContainerEntry() = default;

std::unique_ptr<SvodContainerEntry> SvodContainerEntry::Create(
    Device* device, Entry* parent, const std::string_view name,
    MultiFileHandles* files) {
  const std::string_view path =
      xe::utf8::join_guest_paths(parent->path(), name);
  auto entry =
      std::make_unique<SvodContainerEntry>(device, parent, path, files);

  entry->create_timestamp_ = entry->write_timestamp_ =
      Clock::QueryHostStfsTime();

  return std::move(entry);
}

X_STATUS SvodContainerEntry::Open(uint32_t desired_access, File** out_file) {
  *out_file = new SvodContainerFile(this);
  return X_STATUS_SUCCESS;
}


}  // namespace vfs
}  // namespace xe