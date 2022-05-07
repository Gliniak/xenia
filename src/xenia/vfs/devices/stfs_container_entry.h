/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_STFS_CONTAINER_ENTRY_H_
#define XENIA_VFS_DEVICES_STFS_CONTAINER_ENTRY_H_

#include <map>
#include <string>
#include <vector>

#include "xenia/vfs/entry.h"
#include "xenia/vfs/file.h"

namespace xe {
namespace vfs {
typedef std::map<size_t, FILE*> MultiFileHandles;

class StfsContainerDevice;

class StfsContainerEntry : public Entry {
 public:
  struct BlockRecord {
    size_t file;
    size_t offset;
    size_t length;
  };

  StfsContainerEntry(Device* device, Entry* parent, const std::string_view path,
                     MultiFileHandles* files);
  ~StfsContainerEntry() override;

  static std::unique_ptr<StfsContainerEntry> Create(Device* device,
                                                    Entry* parent,
                                                    const std::string_view name,
                                                    MultiFileHandles* files);
  X_STATUS Open(uint32_t desired_access, File** out_file) override;

  MultiFileHandles* files() const { return files_; }
  uint32_t start_block() const { return start_block_; }
  std::vector<BlockRecord> block_list() {
    if (block_list_.size() <= 0) {
      UpdateBlockList();
    }
    return block_list_;
  }

  bool set_length(uint32_t new_length);
  bool is_read_only();
  bool is_device_closed() { return device_closed_; }

  void mark_dirty() { is_dirty_ = true; }

 private:
  friend class StfsContainerDevice;

  std::unique_ptr<Entry> CreateEntryInternal(const std::string_view name,
                                             uint32_t attributes) override;
  bool DeleteEntryInternal(Entry* entry) override;

  void UpdateBlockList();
  void UpdateBlockList(const std::vector<uint32_t>& block_chain);

  MultiFileHandles* files_;

  // Operations performed with start_block_ = -1 will allocate a new block for
  // us first
  uint32_t start_block_ = -1;

  // If any writes have happened to the file, mark it dirty so we can rehash the
  // blocks for it
  bool is_dirty_ = false;

  bool device_closed_ = false;

  std::vector<BlockRecord> block_list_;
};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_STFS_CONTAINER_ENTRY_H_