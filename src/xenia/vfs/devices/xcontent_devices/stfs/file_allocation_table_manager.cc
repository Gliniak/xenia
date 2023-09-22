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

#include "xenia/base/clock.h"
#include "xenia/base/logging.h"

#include "xenia/vfs/devices/xcontent_devices/stfs/file_allocation_table_manager.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_device.h"

namespace xe {
namespace vfs {
FileAllocationTableManager::FileAllocationTableManager(
    filesystem::File* file, HashBlockManager* hash_manager,
    StfsContainerDevice* device,
    VolumeDescriptor* volume_descriptor)
    : file_(file),
      hash_manager_(hash_manager),
      device_(device),
      volume_descriptor_(volume_descriptor) {
  LoadFileAllocationTableFromFile();
}

FileAllocationTableManager::~FileAllocationTableManager() {}

void FileAllocationTableManager::LoadFileAllocationTableFromFile() {
  const std::vector<uint32_t> indexes = GetAllFileAllocationTableBlocks();

  if (indexes.empty()) {
    return;
  }

  if (indexes.size() != volume_descriptor_->stfs.file_table_block_count) {
    assert_always();
  }

  for (uint32_t index : indexes) {
    const uint64_t file_offset = StfsContainerDevice::BlockIndexToFileOffset(
        index, hash_manager_->GetBlocksPerHashTable());

    StfsDirectoryBlock fat_block = StfsDirectoryBlock();

    file_->Seek(file_offset, SEEK_SET);
    file_->Read(&file_allocation_table_[file_offset],
                sizeof(StfsDirectoryBlock), 1);
  }

  XELOGI(
      "Finished preloading package File Allocation Table from {} blocks "
      "(expected: {})",
      indexes.size(), volume_descriptor_->stfs.file_table_block_count);
}

std::vector<uint32_t>
FileAllocationTableManager::GetAllFileAllocationTableBlocks() {
  std::vector<uint32_t> block_indexes;

  if (!volume_descriptor_->stfs.file_table_block_count) {
    return block_indexes;
  }

  uint32_t fat_block = volume_descriptor_->stfs.file_table_block_number();

  do {
    block_indexes.push_back(fat_block);

    // Find next block index
    auto block_hash = hash_manager_->GetBlockHashEntry(fat_block);
    fat_block = block_hash->level0_next_block();
  } while (fat_block != kEndOfChain);

  return block_indexes;
}

std::vector<StfsDirectoryEntry*> FileAllocationTableManager::GetFiles() {
  std::vector<StfsDirectoryEntry*> files;

  for (auto& block : file_allocation_table_) {
    for (uint8_t i = 0; i < kEntriesPerDirectoryBlock; i++) {
      if (block.second.entries->name[0] == ' ') {
        break;
      }

      files.push_back(&block.second.entries[i]);
    }
  }

  return files;
}

void FileAllocationTableManager::AddFile(const std::string_view name,
                                         uint32_t attributes) {
  if (!IsAnyFileAllocationTableAvailable()) {
    // If there is no table available create new one
    CreateNewFileAllocationTableBlock();
  }

  StfsDirectoryEntry entry = StfsDirectoryEntry();

  strcpy_s(entry.name, name.data());
  entry.flags.name_length = std::min(name.size(), (size_t)40);

  entry.flags.directory = attributes & kFileAttributeDirectory;

  auto [current_date, current_time] =
      encode_fat_timestamp(Clock::QueryHostStfsTime());

  entry.create_date = entry.modified_date = current_date;
  entry.create_time = entry.modified_time = current_time;

  StfsDirectoryEntry* fat_entry = GetFreeFileAllocationTableBlockEntry();
  if (!fat_entry) {
    return;
  }

  *fat_entry = entry;
}

void FileAllocationTableManager::CreateNewFileAllocationTableBlock() {
  const uint32_t fat_block_index =
      device_->AllocateBlock();
  const uint64_t file_offset = StfsContainerDevice::BlockIndexToFileOffset(
      fat_block_index, hash_manager_->GetBlocksPerHashTable());

  file_allocation_table_[file_offset] = StfsDirectoryBlock();
  // TODO: Write to file here?

  volume_descriptor_->stfs.file_table_block_count++;

  if (file_allocation_table_.size() == 1) {
    store_uint24_le(volume_descriptor_->stfs.file_table_block_number_raw,
                    fat_block_index);
  }

}

StfsDirectoryEntry*
FileAllocationTableManager::GetFreeFileAllocationTableBlockEntry() {
  for (auto& fat_block : file_allocation_table_) {
    for (uint8_t i = 0; i < kEntriesPerDirectoryBlock; i++) {
      StfsDirectoryEntry* entry = &fat_block.second.entries[i];

      if (entry->name[0] == 0) {
        return entry;
      }
    }
  }
  XELOGE(
      "GetFreeFileAllocationTableBlockEntry: No free space in any available "
      "FAT block! Crashing probably.");
  return nullptr;
}

}
}  // namespace xe