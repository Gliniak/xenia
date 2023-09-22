/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/filesystem.h"

#include <algorithm>

namespace xe {
namespace filesystem {

bool CreateParentFolder(const std::filesystem::path& path) {
  if (path.has_parent_path()) {
    auto parent_path = path.parent_path();
    if (!std::filesystem::exists(parent_path)) {
      return std::filesystem::create_directories(parent_path);
    }
  }
  return true;
}

// FileOffset Class
std::unique_ptr<File> File::Open(const std::filesystem::path& path,
                                             const std::string_view mode,
                                             const uint32_t start_offset) {
  const size_t file_size = std::filesystem::file_size(path);
  if (start_offset > file_size) {
    return nullptr;
  }

  std::unique_ptr<File> file = std::make_unique<File>();
  file->file_ = OpenFile(path, mode);
  file->path_ = path;
  file->start_offset_ = start_offset;
  file->total_size_ = file_size - start_offset;

  filesystem::Seek(file->file_, start_offset, SEEK_SET);
  return file;
}

bool File::Resize(int64_t new_size) {
  total_size_ = new_size;
  return xe::filesystem::TruncateStdioFile(file_, new_size + start_offset_);
}

bool File::Seek(int64_t offset, int origin) {
  if (offset > (int64_t)total_size_) {
    return false;
  }

  if (origin == SEEK_END) {
    // NOT SUPPORTED?
    return false;
  }

  int64_t proper_offset = offset;

  if (origin == SEEK_SET) {
    proper_offset += start_offset_;
  }

  return filesystem::Seek(file_, proper_offset, origin);
}

size_t File::Read(void* buffer, size_t element_size, size_t element_count) {
  return fread(buffer, element_size, element_count, file_);
}
size_t File::Write(void* buffer, size_t element_size, size_t element_count) {
  return fwrite(buffer, element_size, element_count, file_);
}

void File::Close() { 
    fclose(file_); 
    delete this;
}

}  // namespace filesystem
}  // namespace xe
