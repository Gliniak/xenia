/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_entry.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_device.h"
#include "xenia/vfs/devices/xcontent_devices/stfs/stfs_container_file.h"

#include "xenia/base/clock.h"
#include "xenia/base/math.h"

#include <map>

namespace xe {
namespace vfs {

StfsContainerEntry::StfsContainerEntry(Device* device, Entry* parent,
                                       const std::string_view path)
    : XContentContainerEntry(device, parent, path),
      start_block_(-1),
      is_dirty_(false),
      device_closed_(false) {}

StfsContainerEntry::~StfsContainerEntry() = default;

std::unique_ptr<StfsContainerEntry> StfsContainerEntry::Create(
    Device* device, Entry* parent, const std::string_view name) {
  const std::string path =
      xe::utf8::join_guest_paths(parent->path(), name);
  auto entry =
      std::make_unique<StfsContainerEntry>(device, parent, path);

  entry->create_timestamp_ = entry->write_timestamp_ =
      Clock::QueryHostStfsTime();

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

  auto device = reinterpret_cast<StfsContainerDevice*>(device_);
  if (device->is_read_only()) {
    return false;
  }

  if (new_length == 0) {
    size_ = 0;
    block_list_.clear();
    start_block_ = -1;
    return true;
  }

  if (start_block_ == -1 && new_length > 0) {
    start_block_ = device->AllocateBlock();
  }

  std::vector<uint32_t> block_chain;

  if (start_block_ != -1) {
    block_chain = device->ResizeDataBlockChain(
        start_block_, device->bytes_to_stfs_blocks(new_length));
  }

  size_ = new_length;
  UpdateBlockList(block_chain);

  return true;
}

bool StfsContainerEntry::is_read_only() {
  auto device = reinterpret_cast<StfsContainerDevice*>(device_);
  return device->is_read_only();
}

void StfsContainerEntry::UpdateBlockList() {
  auto device = reinterpret_cast<StfsContainerDevice*>(device_);
  auto block_chain = std::vector<uint32_t>();
  //device->GetDataBlockChain(
  //    start_block_, StfsContainerDevice::bytes_to_stfs_blocks(size_));
  UpdateBlockList(block_chain);
}

void StfsContainerEntry::UpdateBlockList(
    const std::vector<uint32_t>& block_chain) {
  const StfsContainerDevice* device =
      reinterpret_cast<StfsContainerDevice*>(device_);

  auto remaining_length = size_;
  block_list_.clear();
  for (auto block : block_chain) {
    const uint32_t block_size = std::min(uint32_t(remaining_length),
                                         XContentContainerDevice::kBlockSize);
    remaining_length -= block_size;
    BlockRecord record;
    record.file = 0;
    record.offset = device->BlockIndexToFileOffset(block);
    record.length = block_size;
    block_list_.push_back(record);
  }
}

std::unique_ptr<Entry> StfsContainerEntry::CreateEntryInternal(
    const std::string_view name, uint32_t attributes) {
  if (is_read_only()) {
    return nullptr;
  }

  std::unique_ptr<StfsContainerEntry> entry =
      StfsContainerEntry::Create(device_, this, name);



  ((StfsContainerDevice*)device_)
      ->AddFileToFileAllocationTable(name, attributes);
  // Check if there is FAT available. If not create one.

  // Check if there is free space in FAT available. If not create a new one.

  // Check if there is hash block

  return std::unique_ptr<Entry>(
      StfsContainerEntry::Create(device_, this, name));
}

bool StfsContainerEntry::DeleteEntryInternal(Entry* entry) {
  if (is_read_only()) {
    return false;
  }

  // Free any blocks used by the entry
  auto xcontent_entry = reinterpret_cast<StfsContainerEntry*>(entry);
  xcontent_entry->set_length(0);

  return true;
}

}  // namespace vfs
}  // namespace xe