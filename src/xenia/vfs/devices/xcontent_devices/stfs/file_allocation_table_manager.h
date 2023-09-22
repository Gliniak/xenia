/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_XCONTENT_DEVICES_STFS_FILE_ALLOCATION_TABLE_MANAGER_H_
#define XENIA_VFS_DEVICES_XCONTENT_DEVICES_STFS_FILE_ALLOCATION_TABLE_MANAGER_H_ 

#include <string>
#include <unordered_map>

#include "xenia/base/string_util.h"
#include "xenia/kernel/util/xdbf_utils.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/vfs/device.h"
#include "xenia/vfs/devices/stfs_xbox.h"
#include "xenia/vfs/devices/xcontent_container_device.h"
#include "xenia/vfs/devices/xcontent_container_entry.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_device.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/hash_block_manager.h"

namespace xe {
namespace vfs {
class FileAllocationTableManager {
 public:
  static constexpr uint32_t kEntriesPerDirectoryBlock =
      XContentContainerDevice::kBlockSize / sizeof(StfsDirectoryEntry);

  FileAllocationTableManager(filesystem::File* file,
                             HashBlockManager* hash_manager,
                             StfsContainerDevice* device,
                             VolumeDescriptor* volume_descriptor);
  ~FileAllocationTableManager();

  std::vector<StfsDirectoryEntry*> GetFiles();

  void AddFile(const std::string_view name, uint32_t attributes);

 private:
  void LoadFileAllocationTableFromFile();
  void CreateNewFileAllocationTableBlock();

  bool IsAnyFileAllocationTableAvailable() {
    return file_allocation_table_.size();
  }

  StfsDirectoryEntry* GetFreeFileAllocationTableBlockEntry();

  std::vector<uint32_t> GetAllFileAllocationTableBlocks();
  // Offset, block
  std::map<uint64_t, StfsDirectoryBlock> file_allocation_table_;

  std::vector<uint64_t> file_allocation_table_blocks_offsets;
  HashBlockManager* hash_manager_;
  filesystem::File* file_;

  StfsContainerDevice* device_;
  VolumeDescriptor* volume_descriptor_;
};

}
}  // namespace xe

#endif
