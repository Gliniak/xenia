/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_XCONTENT_DEVICES_SVOD_CONTAINER_FILE_H_
#define XENIA_VFS_DEVICES_XCONTENT_DEVICES_SVOD_CONTAINER_FILE_H_

#include "xenia/vfs/file.h"
#include "xenia/vfs/devices/xcontent_container_file.h"
#include "xenia/vfs/devices/xcontent_devices/svod/svod_container_entry.h"

#include "xenia/xbox.h"

namespace xe {
namespace vfs {

class SvodContainerFile : public XContentContainerFile {
 public:
  SvodContainerFile(SvodContainerEntry* entry);
  ~SvodContainerFile() override;

  void Destroy() override;
  X_STATUS ReadSync(void* buffer, size_t buffer_length, size_t byte_offset,
                    size_t* out_bytes_read) override;
 private:
  SvodContainerEntry* entry_;
};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_XCONTENT_DEVICES_STFS_CONTAINER_FILE_H_
