/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <cstring>

#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xam/user_profile.h"
#include "xenia/kernel/xam/xam_private.h"
#include "xenia/kernel/xenumerator.h"
#include "xenia/kernel/xthread.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
namespace xam {

dword_result_t XamParseGamerTileKey_entry(lpdword_t key_ptr, lpdword_t out1_ptr,
                                          lpdword_t out2_ptr,
                                          lpdword_t out3_ptr) {
  *out1_ptr = 0xC0DE0001;
  *out2_ptr = 0xC0DE0002;
  *out3_ptr = 0xC0DE0003;
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamParseGamerTileKey, kUserProfiles, kStub);

dword_result_t XamReadTile_entry(dword_t tile_type, dword_t game_id,
                                 qword_t item_id, dword_t offset,
                                 lpdword_t output_ptr,
                                 lpdword_t buffer_size_ptr,
                                 pointer_t<XAM_OVERLAPPED> overlapped) {
  if (!output_ptr || !buffer_size_ptr) {
    return X_ERROR_FILE_NOT_FOUND;
  }

  auto main_fn = [tile_type, game_id, item_id, offset, output_ptr,
                  buffer_size_ptr]() {
    // item_id can be also profile_xuid!
    uint64_t image_id = item_id;

    uint8_t* data = nullptr;
    size_t data_len = 0;
    std::unique_ptr<MappedMemory> mmap;

    auto type = (XTileType)tile_type.value();
    auto tile_name = kTileFileNames.find(type);

    if (tile_name != kTileFileNames.cend()) {
      const std::string guest_path =
          fmt::format("{:016X}", image_id) + ":\\" + tile_name->second;

      xe::vfs::File* output_file;
      xe::vfs::FileAction action = {};
      auto status = kernel_state()->file_system()->OpenFile(
          nullptr, guest_path, xe::vfs::FileDisposition::kOpen,
          xe::vfs::FileAccess::kFileReadData, false, true, &output_file,
          &action);

      if (XFAILED(status)) {
        return X_ERROR_FILE_NOT_FOUND;
      }

      if (output_file && output_file->entry()) {
        std::vector<uint8_t> file_data;
        file_data.resize(output_file->entry()->size());

        size_t bytes_read = 0;
        output_file->ReadSync(file_data.data(), output_file->entry()->size(), 0,
                              &bytes_read);
        data = file_data.data();
        data_len = bytes_read;
      }
    } else {
      auto gpd = kernel_state()
                     ->profile_manager()
                     ->GetCurrentlyLoggedProfile()
                     ->GetTitleGpd(game_id.value());

      if (!gpd) {
        return X_ERROR_FILE_NOT_FOUND;
      }

      auto entry = gpd->GetEntry(xdbf::XdbfSection::kImage, image_id);

      if (!entry) {
        return X_ERROR_FILE_NOT_FOUND;
      }

      data = entry->data.data();
      data_len = entry->data.size();
    }

    if (!data || !data_len) {
      return X_ERROR_FILE_NOT_FOUND;
    }

    auto passed_size = *buffer_size_ptr;
    *buffer_size_ptr = (uint32_t)data_len;

    auto ret_val = X_ERROR_INVALID_PARAMETER;

    if (passed_size >= *buffer_size_ptr) {
      memcpy_s(output_ptr, *buffer_size_ptr, data, data_len);
      ret_val = X_ERROR_SUCCESS;
    }
    return ret_val;
  };

  auto retval = main_fn();

  if (overlapped) {
    kernel_state()->CompleteOverlappedImmediate(overlapped, retval);
    return X_ERROR_IO_PENDING;
  }

  return retval;
}
DECLARE_XAM_EXPORT1(XamReadTile, kUserProfiles, kSketchy);

dword_result_t XamReadTileEx_entry(dword_t tile_type, dword_t game_id,
                                   qword_t item_id, dword_t offset,
                                   dword_t unk1, dword_t unk2,
                                   lpdword_t output_ptr,
                                   lpdword_t buffer_size_ptr) {
  return XamReadTile_entry(tile_type, game_id, item_id, offset, output_ptr,
                           buffer_size_ptr, 0);
}
DECLARE_XAM_EXPORT1(XamReadTileEx, kUserProfiles, kSketchy);

dword_result_t XamReadTileToTexture_entry(dword_t unknown, dword_t title_id,
                                          qword_t tile_id, dword_t user_index,
                                          lpvoid_t buffer_ptr, dword_t stride,
                                          dword_t height,
                                          dword_t overlapped_ptr) {
  // TODO(gibbed): unknown=0,2,3,9
  if (!tile_id) {
    return X_ERROR_INVALID_PARAMETER;
  }

  size_t size = size_t(stride) * size_t(height);
  std::memset(buffer_ptr, 0xFF, size);

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr,
                                                X_ERROR_SUCCESS);
    return X_ERROR_IO_PENDING;
  }
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamReadTileToTexture, kUserProfiles, kStub);

dword_result_t XamWriteGamerTile_entry(dword_t user_index, dword_t title_id,
                                       dword_t item_id, dword_t arg4, dword_t arg5,
                                       dword_t overlapped_ptr) {
  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr,
                                                X_ERROR_SUCCESS);
    return X_ERROR_IO_PENDING;
  }
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamWriteGamerTile, kUserProfiles, kStub);

}
}  // namespace kernel
}  // namespace xe

DECLARE_XAM_EMPTY_REGISTER_EXPORTS(Misc);