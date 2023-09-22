/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_XCONTENT_DEVICES_STFS_HASH_BLOCK_MANAGER_H_
#define XENIA_VFS_DEVICES_XCONTENT_DEVICES_STFS_HASH_BLOCK_MANAGER_H_

#include <string>
#include <unordered_map>

#include "xenia/base/string_util.h"
#include "xenia/kernel/util/xdbf_utils.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/vfs/device.h"
#include "xenia/vfs/devices/stfs_xbox.h"
#include "xenia/vfs/devices/xcontent_container_device.h"
#include "xenia/vfs/devices/xcontent_container_entry.h"

namespace xe {
namespace vfs {
class HashBlockManager {
 public:
  HashBlockManager(filesystem::File* file, uint8_t blocks_per_hash_table,
                   VolumeDescriptor* volume_descriptor);
  ~HashBlockManager();

  uint8_t GetBlocksPerHashTable() { return blocks_per_hash_table_; }

  StfsHashEntry* GetBlockHashEntry(uint32_t block_index);

  void CalculateHashForBlock(uint32_t block_index);
  void CalculateHashForBlocks(std::vector<uint32_t> blocks_index);

  void WriteToFile(std::vector<uint32_t> blocks_index);
  bool AddHashBlock(uint32_t block_index);

  bool AddNewEntryToHashTable(uint32_t block_index);

  bool IsAnyHashTableAvailable() { return !hash_blocks_.empty(); }
 private:
  // StfsHashEntry GetHashEntryFromFile(uint32_t block_index);

  StfsHashTable GetTableFromBlock(uint32_t block_index);
  StfsHashTable GetTableFromOffset(uint64_t file_offset);

  std::vector<uint64_t> FindAndGetAllHashTableBlockOffsets();

  uint32_t GetHashEntryIndexFromBlockIndex(uint32_t block_index);
  uint32_t GetHashTableOffsetFromBlockIndex(uint32_t block_index);

  uint32_t GetHashBlockOffset(uint32_t block_index, uint8_t hash_level);
  uint32_t GetHashBlockOffsetForLevel0(uint32_t block_index);
  uint32_t GetHashBlockOffsetForLevel1(uint32_t block_index);
  uint32_t GetHashBlockOffsetForLevel2();

  uint8_t GetHighestHashLevelUsed();
  uint8_t GetBlockHashLevel(uint32_t block_index);

  filesystem::File* file_;

  // Secondary block is ONLY used for first level hash table.
  // Remaining blocks are stored normally.
  VolumeDescriptor* volume_descriptor_;
  uint8_t blocks_per_hash_table_;
  // IT APPLIES ONLY FOR FIRST HASH BLOCK?!
  //
  // For writable packages with secondary block being used it always uses 2
  // blocks First block probably stores previous state and second block current
  // state.
  //
  // So to prevent confusion let's always assume we're using first block for
  // offset reference. Then internally copy second block status to first one and
  // update second block. std::map<block_index, HashBlock>
  std::map<uint32_t, StfsHashTable> hash_blocks_;
};
}
}  // namespace xe

#endif