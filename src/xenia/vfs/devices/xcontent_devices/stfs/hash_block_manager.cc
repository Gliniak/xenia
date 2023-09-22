/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <algorithm>
#include <vector>

#include "third_party/crypto/TinySHA1.hpp"

#include "xenia/base/logging.h"

#include "xenia/vfs/devices/xcontent_devices/stfs/hash_block_manager.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_device.h"

namespace xe {
namespace vfs {

HashBlockManager::HashBlockManager(filesystem::File* file,
                                   uint8_t blocks_per_hash_table,
                                   VolumeDescriptor* volume_descriptor)
    : file_(file),
      blocks_per_hash_table_(blocks_per_hash_table),
      volume_descriptor_(volume_descriptor) {}

HashBlockManager::~HashBlockManager() {}

uint32_t HashBlockManager::GetHashTableOffsetFromBlockIndex(
    uint32_t block_index) {
  return GetHashBlockOffset(block_index, 0);
}

uint32_t HashBlockManager::GetHashEntryIndexFromBlockIndex(
    uint32_t block_index) {
  return block_index % kBlocksPerHashLevel[0];
}

StfsHashEntry* HashBlockManager::GetBlockHashEntry(uint32_t block_index) {
  const uint32_t hash_table_offset =
      GetHashTableOffsetFromBlockIndex(block_index);
  const uint32_t hash_entry_index =
      GetHashEntryIndexFromBlockIndex(block_index);

  if (hash_blocks_.find(hash_table_offset) == hash_blocks_.cend()) {
    // Load block from file
    hash_blocks_[hash_table_offset] = GetTableFromOffset(hash_table_offset);
  }
  return &hash_blocks_[hash_table_offset].entries[hash_entry_index];
}

uint32_t HashBlockManager::GetHashBlockOffset(uint32_t block_index,
                                              uint8_t hash_level) {
  uint32_t hash_block_offset = 0;
  switch (hash_level) {
    case 0:
      hash_block_offset = GetHashBlockOffsetForLevel0(block_index);
      break;
    case 1:
      hash_block_offset = GetHashBlockOffsetForLevel1(block_index);
      break;
    case 2:
      hash_block_offset = GetHashBlockOffsetForLevel2();
      break;
    default:
      break;
  }

  // Check if secondary block is used
  uint32_t secondary_block_offset = 0;
  // if (volume_descriptor_->stfs.flags.bits.read_only_format)

  return hash_block_offset + secondary_block_offset;
}

uint32_t HashBlockManager::GetHashBlockOffsetForLevel0(uint32_t block_index) {
  if (block_index < kBlocksPerHashLevel[0]) {
    return 0;
  }

  uint32_t block = (block_index / kBlocksPerHashLevel[0]) *
                   StfsContainerDevice::GetBlockStep(0, blocks_per_hash_table_);
  block +=
      ((block_index / kBlocksPerHashLevel[1]) + 1) * blocks_per_hash_table_;

  if (block_index < kBlocksPerHashLevel[1]) {
    return block * XContentContainerDevice::kBlockSize;
  }

  return (block + blocks_per_hash_table_) * XContentContainerDevice::kBlockSize;
}

uint32_t HashBlockManager::GetHashBlockOffsetForLevel1(uint32_t block_index) {
  if (block_index < kBlocksPerHashLevel[1]) {
    return StfsContainerDevice::GetBlockStep(0, blocks_per_hash_table_) *
           XContentContainerDevice::kBlockSize;
  }

  uint32_t block = (block_index / kBlocksPerHashLevel[1]) *
                   StfsContainerDevice::GetBlockStep(1, blocks_per_hash_table_);
  return (block + blocks_per_hash_table_) * XContentContainerDevice::kBlockSize;
}

uint32_t HashBlockManager::GetHashBlockOffsetForLevel2() {
  return StfsContainerDevice::GetBlockStep(1, blocks_per_hash_table_) *
         XContentContainerDevice::kBlockSize;
}

uint8_t HashBlockManager::GetHighestHashLevelUsed() {
  for (uint8_t i = 0; i < kBlocksHashLevelAmount; i++) {
    if (volume_descriptor_->stfs.total_block_count <= kBlocksPerHashLevel[i]) {
      return i;
    }
  }
  return 2;
}

uint8_t HashBlockManager::GetBlockHashLevel(uint32_t block_index) {
  if (block_index < kBlocksPerHashLevel[1]) {
    return 0;
  }

  if (block_index < kBlocksPerHashLevel[2]) {
    return 1;
  }
  return 2;
}

void HashBlockManager::CalculateHashForBlock(uint32_t block_index) {}

StfsHashTable HashBlockManager::GetTableFromBlock(uint32_t block_index) {
  const uint64_t offset = StfsContainerDevice::BlockIndexToFileOffset(
      block_index, blocks_per_hash_table_);

  return GetTableFromOffset(offset);
}

StfsHashTable HashBlockManager::GetTableFromOffset(uint64_t file_offset) {
  StfsHashTable block = StfsHashTable();
  file_->Seek(file_offset, SEEK_SET);
  file_->Read(&block, sizeof(StfsHashTable), 1);

  return block;
}

std::vector<uint64_t> HashBlockManager::FindAndGetAllHashTableBlockOffsets() {
  std::vector<uint64_t> offsets;

  // No blocks allocated, so no hash table
  if (!volume_descriptor_->stfs.total_block_count) {
    return offsets;
  }

  // always add 1st hash block
  offsets.push_back(GetHashBlockOffset(
      volume_descriptor_->stfs.file_table_block_number(), 0));

  uint32_t remaining_blocks = volume_descriptor_->stfs.total_block_count - 1;

  // Level 1 and level 2 blocks are different depending if package is writable
  // or not
  if (volume_descriptor_->stfs.flags.bits.read_only_format) {
    const uint32_t total_block_count_in_hash_tables =
        remaining_blocks * kBlocksPerHashLevel[0];

    // If package is read-only all hash blocks are 170 blocks in size
    for (uint32_t i = kBlocksPerHashLevel[0];
         i < total_block_count_in_hash_tables; i += kBlocksPerHashLevel[0]) {
      offsets.push_back(GetHashBlockOffset(i, 0));
    }

    return offsets;
  }

  const uint8_t highest_level = GetHighestHashLevelUsed();
  // TODO: FIX THESE!
  if (highest_level >= 1) {
    for (uint32_t i = kBlocksPerHashLevel[0];
         i < kBlocksPerHashLevel[1] && remaining_blocks > 0;
         i += kBlocksPerHashLevel[0], remaining_blocks--) {
      offsets.push_back(GetHashBlockOffset(i, 0));
    }
  }

  if (highest_level == 2) {
    for (uint32_t i = kBlocksPerHashLevel[1];
         i < kBlocksPerHashLevel[2] && remaining_blocks > 0;
         i += kBlocksPerHashLevel[0], remaining_blocks--) {
      offsets.push_back(GetHashBlockOffset(i, 0));
    }
  }
  return offsets;
}

}
}  // namespace xe