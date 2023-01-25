/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_
#define XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "xenia/base/math.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/kernel/xam/content_manager.h"
#include "xenia/vfs/device.h"
#include "xenia/vfs/devices/stfs_xbox.h"

#if XE_PLATFORM_WIN32
#include "xenia/base/platform_win.h"
#endif

namespace xe {
namespace vfs {

static uint64_t decode_fat_timestamp(uint16_t date, uint16_t time) {
  struct tm tm = {0};
  // 80 is the difference between 1980 (FAT) and 1900 (tm);
  tm.tm_year = ((0xFE00 & date) >> 9) + 80;
  tm.tm_mon = ((0x01E0 & date) >> 5) - 1;
  tm.tm_mday = (0x001F & date) >> 0;
  tm.tm_hour = (0xF800 & time) >> 11;
  tm.tm_min = (0x07E0 & time) >> 5;
  tm.tm_sec = (0x001F & time) << 1;  // the value stored in 2-seconds intervals
  tm.tm_isdst = -1;
#if XE_PLATFORM_WIN32
  time_t timet = _mkgmtime(&tm);
#else
  time_t timet = timegm(&tm);
#endif
  if (timet == -1) {
    return 0;
  }
  // 11644473600LL is a difference between 1970 and 1601
  return (timet + 11644473600LL) * 10000000;
}
static std::tuple<uint16_t, uint16_t> encode_fat_timestamp(uint64_t timestamp) {
  time_t time_ = (timestamp / 10000000) - 11644473600LL;
  // Workaround for unset timestamps
  if (!timestamp) {
    time_ = 0;
  }
  auto* tm = gmtime(&time_);

  uint16_t date = ((tm->tm_year - 80) << 9);
  date |= ((tm->tm_mon + 1) << 5);
  date |= (tm->tm_mday << 0);

  uint16_t time = (tm->tm_hour << 11);
  time |= (tm->tm_min << 5);
  time |= (tm->tm_sec >> 1);

  return std::make_tuple(date, time);
}

// https://free60project.github.io/wiki/STFS.html

class StfsContainerEntry;

class StfsContainerDevice : public Device {
 public:
  const static uint32_t kBlockSize = 0x1000;

  StfsContainerDevice(const std::string_view mount_path,
                      const std::filesystem::path& host_path,
                      bool read_only = false);
  ~StfsContainerDevice() override;

  bool Initialize() override;

  bool is_read_only() const override {
    return !allow_writing_ ||
           header_.metadata.volume_type != XContentVolumeType::kStfs ||
           header_.metadata.volume_descriptor.stfs.flags.bits.read_only_format;
  }

  void Dump(StringBuffer* string_buffer) override;
  Entry* ResolvePath(const std::string_view path) override;

  const std::string& name() const override { return name_; }
  uint32_t attributes() const override { return 0; }
  uint32_t component_name_max_length() const override { return 40; }

  uint32_t total_allocation_units() const override {
    if (header_.metadata.volume_type == XContentVolumeType::kStfs) {
      return header_.metadata.volume_descriptor.stfs.total_block_count;
    }

    return uint32_t(data_size() / sectors_per_allocation_unit() /
                    bytes_per_sector());
  }
  uint32_t available_allocation_units() const override {
    if (!is_read_only()) {
      auto& descriptor = header_.metadata.volume_descriptor.stfs;
      return kBlocksPerHashLevel[2] -
             (descriptor.total_block_count - descriptor.free_block_count);
    }
    return 0;
  }
  uint32_t sectors_per_allocation_unit() const override { return 8; }
  uint32_t bytes_per_sector() const override { return 0x200; }

  size_t data_size() const {
    if (header_.package_header.header_size) {
      if (header_.metadata.volume_type == XContentVolumeType::kStfs) {
        return header_.metadata.volume_descriptor.stfs.total_block_count *
               kBlockSize;
      }
      return files_total_size_ -
             xe::round_up(header_.package_header.header_size, kBlockSize);
    }
    return files_total_size_ - sizeof(XContentHeader);
  }

  static uint32_t bytes_to_stfs_blocks(size_t num_bytes) {
    // xe::round_up doesn't handle 0 how we need it to, so:
    return uint32_t((num_bytes + kBlockSize - 1) / kBlockSize);
  }

  uint32_t STFSMaxHashLevel() const {
    const uint32_t block_count =
        header_.metadata.volume_descriptor.stfs.total_block_count;

    if (block_count <= kBlocksPerHashLevel[0]) {
      return 0;
    }
    if (block_count <= kBlocksPerHashLevel[1]) {
      return 1;
    }
    return 2;
  }

  XContentHeader& header() { return header_; }
  bool CreateStfsContainer(const kernel::xam::XCONTENT_AGGREGATE_DATA& data,
                           const uint32_t flags);
  // Writes updated headers & hash-tables to the file
  bool STFSFlush();

 protected:
  friend class StfsContainerEntry;

  // STFS Writing related methods
  uint32_t STFSAllocateBlock();

  // Resets hash tables to an empty state
  bool STFSReset();

  // Get all blocks related to provided block_id/block_number
  std::vector<uint32_t> STFSGetDataBlockChain(uint32_t block_num,
                                              uint32_t max_count = 0xFFFFFF);
  std::vector<uint32_t> STFSResizeDataBlockChain(uint32_t start_block,
                                                 uint32_t num_blocks);

 private:
  enum class Error {
    kSuccess = 0,
    kErrorOutOfMemory = -1,
    kErrorReadError = -10,
    kErrorFileMismatch = -30,
    kErrorDamagedFile = -31,
    kErrorTooSmall = -32,
  };

  enum class SvodLayoutType {
    kUnknown = 0x0,
    kEnhancedGDF = 0x1,
    kXSF = 0x2,
    kSingleFile = 0x4,
  };

  static constexpr uint8_t kHashLevels = 3;
  static constexpr uint32_t kBlocksPerHashLevel[kHashLevels] = {170, 28900,
                                                                4913000};
  static constexpr uint32_t kEndOfChain = 0xFFFFFF;
  static constexpr uint32_t kEntriesPerDirectoryBlock =
      kBlockSize / sizeof(StfsDirectoryEntry);

  static XContentPackageType ReadMagic(const std::filesystem::path& path);

  FILE* OpenHostFile();
  FILE* main_file() { return files_.at(0); }

  Error ReadHeaderAndVerify(FILE* header_file);
  Error OpenFiles(FILE* header_file);
  Error MapSVODFiles();
  Error ReadSVOD();
  Error ReadEntrySVOD(uint32_t sector, uint32_t ordinal,
                      StfsContainerEntry* parent);
  void CloseFiles();

  bool ResolveFromFolder(const std::filesystem::path& path);

  // SVOD Saving related methods
  bool STFSCreateHeader(const xe::kernel::xam::XCONTENT_AGGREGATE_DATA& data,
                        const uint32_t flags);

  bool STFSCreateRootDirectory();

  void BlockToOffsetSVOD(size_t sector, size_t* address, size_t* file_index);

  bool STFSDirectoryRead();
  void STFSDirectoryWrite();
  void RemoveFreeTrailingBlocks(StfsVolumeDescriptor& descriptor);

  uint64_t STFSDataBlockToOffset(uint32_t block_num) const;
  uint32_t STFSDataBlockToHashBlockNum(uint32_t block_num,
                                       uint32_t hash_level) const;
  uint64_t STFSDataBlockToHashBlockOffset(uint32_t block_num,
                                          uint32_t hash_level) const;

  StfsHashTable& STFSGetHashTable(uint32_t block_num, uint32_t hash_level,
                                  bool use_secondary_block = false,
                                  uint8_t* hash_in_out = nullptr,
                                  bool* is_table_invalid = nullptr);

  StfsHashEntry& STFSGetHashEntry(uint32_t block_num, uint32_t hash_level,
                                  bool use_secondary_block = false,
                                  uint8_t* hash_in_out = nullptr);

  // DataHash functions handle secondary block & hash checking for us
  StfsHashTable& STFSGetDataHashTable(uint32_t block_num,
                                      bool* is_table_invalid);
  StfsHashEntry STFSGetDataHashEntry(uint32_t block_num);
  void STFSSetDataHashEntry(uint32_t block_num,
                            const StfsHashEntry& hash_entry);

  bool IsWriteToBlockRequired(const StfsDirectoryBlock* directory,
                              const uint32_t current_block);

  void STFSMarkBlockDirty(uint32_t block_num);
  std::vector<uint32_t> STFSAllocateBlocks(uint32_t amount_of_blocks);

  bool STFSIsBlockDirty(uint32_t block_num) const;
  void STFSFreeBlock(uint32_t block_num);
  void STFSSetDataBlockChain(const std::vector<uint32_t>& chain);

  std::string name_;
  std::filesystem::path host_path_;
  bool allow_writing_ = false;
  XContentHeader header_;

  std::map<size_t, FILE*> files_;
  size_t files_total_size_;
  std::unique_ptr<StfsContainerEntry> root_entry_;

  size_t svod_base_offset_;
  SvodLayoutType svod_layout_;

  std::unordered_map<uint64_t, StfsHashTable> hash_tables_;
  std::vector<uint32_t> dirty_blocks_;
  std::vector<uint64_t> invalid_tables_;

  uint32_t blocks_per_hash_table_;
  uint32_t block_step_[2];
};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_STFS_CONTAINER_DEVICE_H_