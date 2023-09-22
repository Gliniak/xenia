/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_XCONTENT_DEVICES_STFS_CONTAINER_DEVICE_H_
#define XENIA_VFS_DEVICES_XCONTENT_DEVICES_STFS_CONTAINER_DEVICE_H_

#include <string>
#include <unordered_map>

#include "xenia/base/string_util.h"
#include "xenia/kernel/util/xdbf_utils.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/vfs/device.h"
#include "xenia/vfs/devices/stfs_xbox.h"
#include "xenia/vfs/devices/xcontent_container_device.h"
#include "xenia/vfs/devices/xcontent_container_entry.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/file_allocation_table_manager.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/hash_block_manager.h"

namespace xe {
namespace vfs {

struct CacheBlock {
  bool is_writable;
  uint8_t block_data[0x1000];
};

// https://free60project.github.io/wiki/STFS.html
class StfsContainerDevice : public XContentContainerDevice {
 public:
  StfsContainerDevice(const std::string_view mount_path,
                      const std::filesystem::path& host_path,
                      std::unique_ptr<filesystem::File>,
                      VolumeDescriptor* volume_descriptor);
  ~StfsContainerDevice();

  bool is_read_only() const override {
    return volume_descriptor_->stfs.flags.bits.read_only_format;
  }

  uint32_t component_name_max_length() const override { return 40; }

  uint32_t total_allocation_units() const override {
    return volume_descriptor_->stfs.total_block_count;
  }

  uint32_t available_allocation_units() const override {
    if (is_read_only()) {
      return 0;
    }

    return kBlocksPerHashLevel[2] -
           (volume_descriptor_->stfs.total_block_count -
            volume_descriptor_->stfs.free_block_count);
  }
  void CloseFiles(){};

  void AddFileToFileAllocationTable(const std::string_view name,
                                    uint32_t attributes);

  static uint64_t BlockIndexToFileOffset(uint32_t block_index,
                                         uint8_t blocks_per_hash_table = 1);
  static uint32_t FileOffsetToBlockIndex(uint32_t file_offset);
  static uint32_t GetBlockStep(uint8_t step_index,
                               uint8_t blocks_per_hash_table);

  CacheBlock* GetCachedBlockEntry(uint64_t offset);
  void AllocateNewCacheBlockEntry(uint64_t offset);

 protected:
  friend class StfsContainerEntry;
  friend class StfsContainerFile;
  friend class FileAllocationTableManager;
  // STFS Writing related methods
  
  uint32_t AllocateBlock(bool use_free_blocks = true);
  // Get all blocks related to provided block_id/block_number
  // std::vector<uint32_t> GetDataBlockChain(uint32_t block_num,
  //                                             uint32_t max_count = 0xFFFFFF);
  std::vector<uint32_t> ResizeDataBlockChain(uint32_t start_block,
                                             uint32_t num_blocks) {
    return std::vector<uint32_t>();
  }

 private:
  Result Read() override;

  bool CreateRootDirectory();

  std::unique_ptr<StfsContainerEntry> ReadEntry(
      Entry* parent, const StfsDirectoryEntry* dir_entry);

  // size_t BlockToOffset(uint64_t block_index) const;

  const bool IsFreeBlockAvailable() const;
  const uint32_t FindFreeBlock();

  static inline uint32_t bytes_to_stfs_blocks(size_t num_bytes) {
    // xe::round_up doesn't handle 0 how we need it to, so:
    return uint32_t((num_bytes + kBlockSize - 1) / kBlockSize);
  }

  bool IsFileAllocationTableAvailable() {
    return volume_descriptor_->stfs.file_table_block_count != 0;
  }

  std::unique_ptr<xe::filesystem::File> file_;

  uint8_t blocks_per_hash_table_;

  std::unique_ptr<HashBlockManager> hash_manager_;
  std::unique_ptr<FileAllocationTableManager> file_allocation_table_manager_;

  std::unordered_map<uint64_t, CacheBlock> cached_blocks_;
};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_XCONTENT_DEVICES_STFS_CONTAINER_DEVICE_H_
