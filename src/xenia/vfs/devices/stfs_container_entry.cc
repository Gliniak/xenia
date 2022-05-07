/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/stfs_container_entry.h"
#include "xenia/base/clock.h"
#include "xenia/base/math.h"
#include "xenia/vfs/devices/stfs_container_device.h"
#include "xenia/vfs/devices/stfs_container_file.h"

#include <map>

namespace xe {
namespace vfs {

StfsContainerEntry::StfsContainerEntry(Device* device, Entry* parent,
                                       const std::string_view path,
                                       MultiFileHandles* files)
    : Entry(device, parent, path),
      files_(files),
      start_block_(-1),
      is_dirty_(false),
      device_closed_(false) {}

StfsContainerEntry::~StfsContainerEntry() = default;

std::unique_ptr<StfsContainerEntry> StfsContainerEntry::Create(
    Device* device, Entry* parent, const std::string_view name,
    MultiFileHandles* files) {
  auto path = xe::utf8::join_guest_paths(parent->path(), name);
  auto entry =
      std::make_unique<StfsContainerEntry>(device, parent, path, files);

  entry->create_timestamp_ = entry->write_timestamp_ =
      Clock::QueryHostSystemTime();

  return std::move(entry);
}

X_STATUS StfsContainerEntry::Open(uint32_t desired_access, File** out_file) {
  *out_file = new StfsContainerFile(desired_access, this);
  return X_STATUS_SUCCESS;
}

bool StfsContainerEntry::set_length(uint32_t new_length) {
  if (new_length == size_) {
    return true;
  }
  if (device_closed_) {
    return false;
  }

  auto device = reinterpret_cast<StfsContainerDevice*>(device_);
  if (device->is_read_only()) {
    return false;
  }

  if (start_block_ == -1 && new_length > 0) {
    start_block_ = device->STFSBlockAllocate();
  }

  std::vector<uint32_t> block_chain;

  if (start_block_ != -1) {
    block_chain = device->STFSResizeDataBlockChain(
        start_block_, device->bytes_to_stfs_blocks(new_length));
  }

  size_ = new_length;
  if (new_length == 0) {
    block_list_.clear();
    start_block_ = -1;
  } else {
    UpdateBlockList(block_chain);
  }

  return true;
}

bool StfsContainerEntry::is_read_only() {
  if (device_closed_) {
    return true;
  }
  auto device = reinterpret_cast<StfsContainerDevice*>(device_);
  return device->is_read_only();
}

void StfsContainerEntry::UpdateBlockList() {
  if (device_closed_) {
    return;
  }
  auto device = reinterpret_cast<StfsContainerDevice*>(device_);
  auto block_chain = device->STFSGetDataBlockChain(
      start_block_, StfsContainerDevice::bytes_to_stfs_blocks(size_));
  UpdateBlockList(block_chain);
}

void StfsContainerEntry::UpdateBlockList(
    const std::vector<uint32_t>& block_chain) {
  if (device_closed_) {
    return;
  }

  auto device = reinterpret_cast<StfsContainerDevice*>(device_);

  auto remaining_length = size_;
  block_list_.clear();
  for (auto block : block_chain) {
    auto block_size = std::min(remaining_length, size_t(0x1000));
    remaining_length -= block_size;
    BlockRecord record;
    record.file = 0;
    record.offset = device->STFSDataBlockToOffset(block);
    record.length = uint32_t(block_size);
    block_list_.push_back(record);
  }
}

std::unique_ptr<Entry> StfsContainerEntry::CreateEntryInternal(
    const std::string_view name, uint32_t attributes) {
  if (is_read_only()) {
    return nullptr;
  }
  if (device_closed_) {
    return nullptr;
  }

  return std::unique_ptr<Entry>(
      StfsContainerEntry::Create(device_, this, name, files_));
}

bool StfsContainerEntry::DeleteEntryInternal(Entry* entry) {
  if (is_read_only()) {
    return false;
  }
  if (device_closed_) {
    return false;
  }

  // If this is root entry & the last file is being deleted:
  if (path_.empty() && children_.size() == 1 && children_[0].get() == entry) {
    // The last file is being deleted from the package root
    // Ask STFS device to reset itself, this'll remove all hash-tables & unused
    // blocks, etc.
    auto device = reinterpret_cast<StfsContainerDevice*>(device_);
    return device->STFSReset();
  }

  // Free any blocks used by the entry
  auto xcontent_entry = reinterpret_cast<StfsContainerEntry*>(entry);
  xcontent_entry->set_length(0);

  return true;
}

}  // namespace vfs
}  // namespace xe