/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <algorithm>
#include <vector>

#include "third_party/crypto/TinySHA1.hpp"

#include "xenia/base/logging.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_device.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_entry.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/file_allocation_table_manager.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/hash_block_manager.h"

namespace xe {
namespace vfs {

StfsContainerDevice::StfsContainerDevice(
    const std::string_view mount_path, const std::filesystem::path& host_path,
    std::unique_ptr<xe::filesystem::File> file,
    VolumeDescriptor* volume_descriptor)
    : XContentContainerDevice(mount_path, host_path, volume_descriptor),
      file_(file.release()),
      blocks_per_hash_table_(1) {
  SetName("STFS");

  blocks_per_hash_table_ = is_read_only() ? 1 : 2;

  XELOGE("StfsContainerDevice - Created Device! Host File: {} Mount Path: {}",
         xe::path_to_utf8(host_path), mount_path);
}

StfsContainerDevice::~StfsContainerDevice() { CloseFiles(); }

StfsContainerDevice::Result StfsContainerDevice::Read() {
  // Root directory must be always available even on freshly created package.
  // This entry will be written later as base of allocation table and will be
  // used as root directory for any file created inside.
  CreateRootDirectory();

  hash_manager_ = std::make_unique<HashBlockManager>(
      file_.get(), blocks_per_hash_table_, volume_descriptor_);

  file_allocation_table_manager_ = std::make_unique<FileAllocationTableManager>(
      file_.get(), hash_manager_.get(), this, volume_descriptor_);
  // This package was probably just created and it doesn't have allocation
  // table!
  if (!IsFileAllocationTableAvailable()) {
    // Table will be created when first file will be created.
    return Result::kSuccess;
  }

  // Create host tree structure of files
  // TODO: DO WE REALLY NEED THIS?
  std::vector<StfsContainerEntry*> all_entries;

  for (StfsDirectoryEntry* file : file_allocation_table_manager_->GetFiles()) {
    XContentContainerEntry* parent_entry =
        file->directory_index == 0xFFFF ? root_entry_.get()
                                        : all_entries[file->directory_index];

    std::unique_ptr<StfsContainerEntry> entry = ReadEntry(parent_entry, file);
    all_entries.push_back(entry.get());
    parent_entry->children_.emplace_back(std::move(entry));
  }

  return Result::kSuccess;
}

std::unique_ptr<StfsContainerEntry> StfsContainerDevice::ReadEntry(
    Entry* parent, const StfsDirectoryEntry* dir_entry) {
  std::string name(reinterpret_cast<const char*>(dir_entry->name),
                   dir_entry->flags.name_length & 0x3F);

  auto entry = StfsContainerEntry::Create(this, parent, name);

  if (dir_entry->flags.directory) {
    entry->attributes_ = kFileAttributeDirectory;
  } else {
    entry->attributes_ = kFileAttributeNormal | kFileAttributeReadOnly;
    entry->data_offset_ = BlockIndexToFileOffset(
        dir_entry->start_block_number(), blocks_per_hash_table_);
    entry->data_size_ = dir_entry->length;
  }
  entry->size_ = dir_entry->length;
  entry->allocation_size_ = xe::round_up(dir_entry->length, kBlockSize);

  entry->create_timestamp_ =
      decode_fat_timestamp(dir_entry->create_date, dir_entry->create_time);
  entry->write_timestamp_ =
      decode_fat_timestamp(dir_entry->modified_date, dir_entry->modified_time);
  entry->access_timestamp_ = entry->write_timestamp_;

  // Fill in all block records.
  // It's easier to do this now and just look them up later, at the cost
  // of some memory. Nasty chain walk.
  // TODO(benvanik): optimize if flags.contiguous is set.
  if (entry->attributes() & X_FILE_ATTRIBUTE_NORMAL) {
    uint32_t block_index = dir_entry->start_block_number();
    size_t remaining_size = dir_entry->length;
    while (remaining_size && block_index != kEndOfChain) {
      size_t block_size =
          std::min(static_cast<size_t>(kBlockSize), remaining_size);
      size_t offset =
          BlockIndexToFileOffset(block_index, blocks_per_hash_table_);
      entry->block_list_.push_back({0, offset, block_size});
      remaining_size -= block_size;
      auto block_hash = hash_manager_->GetBlockHashEntry(block_index);
      block_index = block_hash->level0_next_block();
    }

    if (remaining_size) {
      // Loop above must have exited prematurely, bad hash tables?
      XELOGW(
          "STFS file {} only found {} bytes for file, expected {} ({} "
          "bytes missing)",
          name, dir_entry->length - remaining_size, dir_entry->length,
          remaining_size);
      assert_always();
    }

    // Check that the number of blocks retrieved from hash entries matches
    // the block count read from the file entry
    if (entry->block_list_.size() != dir_entry->allocated_data_blocks()) {
      XELOGW(
          "STFS failed to read correct block-chain for entry {}, read {} "
          "blocks, expected {}",
          entry->name_, entry->block_list_.size(),
          dir_entry->allocated_data_blocks());
      assert_always();
    }
  }

  return entry;
}

bool StfsContainerDevice::CreateRootDirectory() {
  auto root_entry = new StfsContainerEntry(this, nullptr, "");
  root_entry->attributes_ = kFileAttributeDirectory;
  root_entry_ = std::unique_ptr<StfsContainerEntry>(root_entry);
  return true;
}

const uint32_t StfsContainerDevice::FindFreeBlock() {
  if (!volume_descriptor_->stfs.free_block_count) {
    return -1;
  }
  return -1;
}

const bool StfsContainerDevice::IsFreeBlockAvailable() const {
  return volume_descriptor_->stfs.free_block_count > 0;
}

uint32_t StfsContainerDevice::AllocateBlock(bool use_free_blocks) {
  auto global_lock = global_critical_region_.Acquire();

  // Block is free and allocated
  if (IsFreeBlockAvailable() && use_free_blocks) {
    const uint32_t block_num = FindFreeBlock();

    // If for whatever reason block won't be found then go with path that
    // creates new block.
    if (block_num != -1) {
      volume_descriptor_->stfs.free_block_count--;
      return block_num;
    }
  }

  const uint32_t block_num = ++volume_descriptor_->stfs.total_block_count;
  const uint64_t new_container_size =
      BlockIndexToFileOffset(block_num, blocks_per_hash_table_) + kBlockSize;
  file_->Resize(new_container_size);
  return block_num;
}

uint64_t StfsContainerDevice::BlockIndexToFileOffset(
    uint32_t block_index, uint8_t blocks_per_hash_table) {
  // For every level there is a hash table
  // Level 0: hash table of next 170 blocks
  // Level 1: hash table of next 170 hash tables
  // Level 2: hash table of next 170 level 1 hash tables
  // And so on...
  uint64_t block = block_index;
  for (uint32_t i = 0; i < kBlocksHashLevelAmount; i++) {
    const uint32_t level_base = kBlocksPerHashLevel[i];
    block += ((block_index + level_base) / level_base) * blocks_per_hash_table;
    if (block_index < level_base) {
      break;
    }
  }

  return block * XContentContainerDevice::kBlockSize;
}

// Todo: fix it
uint32_t StfsContainerDevice::FileOffsetToBlockIndex(uint32_t file_offset) {
  return file_offset / XContentContainerDevice::kBlockSize;
}

uint32_t StfsContainerDevice::GetBlockStep(uint8_t step_index,
                                           uint8_t blocks_per_hash_table) {
  uint32_t block_step = 0;

  switch (step_index) {
    case 0:
      block_step = kBlocksPerHashLevel[0] + blocks_per_hash_table;
      break;
    case 1:
      block_step = kBlocksPerHashLevel[1] +
                   ((kBlocksPerHashLevel[0] + 1) * blocks_per_hash_table);
      break;
    default:
      XELOGE("StfsContainerDevice::GetBlockStep - Unknown step_index: {}",
             step_index);
      break;
  }
  return block_step;
}

void StfsContainerDevice::AddFileToFileAllocationTable(
    const std::string_view name, uint32_t attributes) {
  file_allocation_table_manager_->AddFile(name, attributes);
}

}  // namespace vfs
}  // namespace xe
