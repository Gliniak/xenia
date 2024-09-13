/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_XCONTENT_H_
#define XENIA_KERNEL_XAM_XCONTENT_H_

#include "xenia/base/string_util.h"
#include "xenia/vfs/devices/stfs_xbox.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
namespace xam {

// If set in XCONTENT_AGGREGATE_DATA, will be substituted with the running
// titles ID
// TODO: check if actual x360 kernel/xam has a value similar to this
constexpr uint32_t kCurrentlyRunningTitleId = 0xFFFFFFFF;

enum class XContentPackageType : uint32_t {
  kCon = 0x434F4E20,
  kPirs = 0x50495253,
  kLive = 0x4C495645,
};

enum class XContentVolumeType : uint32_t {
  kStfs = 0,
  kSvod = 1,
};

struct XContentLicense {
  be<uint64_t> licensee_id;
  be<uint32_t> license_bits;
  be<uint32_t> license_flags;
};
static_assert_size(XContentLicense, 0x10);

struct XContentMediaData {
  uint8_t series_id[0x10];
  uint8_t season_id[0x10];
  be<uint16_t> season_number;
  be<uint16_t> episode_number;
};
static_assert_size(XContentMediaData, 0x24);

struct XContentAvatarAssetData {
  be<uint32_t> sub_category;
  be<uint32_t> colorizable;
  uint8_t asset_id[0x10];
  uint8_t skeleton_version_mask;
  uint8_t reserved[0xB];
};
static_assert_size(XContentAvatarAssetData, 0x24);

struct XContentAttributes {
  uint8_t profile_transfer : 1;
  uint8_t device_transfer : 1;
  uint8_t move_only_transfer : 1;
  uint8_t kinect_enabled : 1;
  uint8_t disable_network_storage : 1;
  uint8_t deep_link_supported : 1;
  uint8_t reserved : 2;
};
static_assert_size(XContentAttributes, 1);

#pragma pack(push, 1)
struct XContentMetadata {
  static const uint32_t kThumbLengthV1 = 0x4000;
  static const uint32_t kThumbLengthV2 = 0x3D00;

  static const uint32_t kNumLanguagesV1 = 9;
  // metadata_version 2 adds 3 languages inside thumbnail/title_thumbnail space
  static const uint32_t kNumLanguagesV2 = 12;

  be<XContentType> content_type;
  be<uint32_t> metadata_version;
  be<uint64_t> content_size;
  xex2_opt_execution_info execution_info;
  uint8_t console_id[5];
  be<uint64_t> profile_id;
  union {
    vfs::StfsVolumeDescriptor stfs;
    vfs::SvodDeviceDescriptor svod;
  } volume_descriptor;
  be<uint32_t> data_file_count;
  be<uint64_t> data_file_size;
  be<XContentVolumeType> volume_type;
  be<uint64_t> online_creator;
  be<uint32_t> category;
  uint8_t reserved2[0x20];
  union {
    XContentMediaData media_data;
    XContentAvatarAssetData avatar_asset_data;
  } metadata_v2;
  uint8_t device_id[0x14];
  union {
    be<uint16_t> uint[kNumLanguagesV1][128];
    char16_t chars[kNumLanguagesV1][128];
  } display_name_raw;
  union {
    be<uint16_t> uint[kNumLanguagesV1][128];
    char16_t chars[kNumLanguagesV1][128];
  } description_raw;
  union {
    be<uint16_t> uint[64];
    char16_t chars[64];
  } publisher_raw;
  union {
    be<uint16_t> uint[64];
    char16_t chars[64];
  } title_name_raw;
  union {
    uint8_t as_byte;
    XContentAttributes bits;
  } flags;
  be<uint32_t> thumbnail_size;
  be<uint32_t> title_thumbnail_size;
  uint8_t thumbnail[kThumbLengthV2];
  union {
    be<uint16_t> uint[kNumLanguagesV2 - kNumLanguagesV1][128];
    char16_t chars[kNumLanguagesV2 - kNumLanguagesV1][128];
  } display_name_ex_raw;
  uint8_t title_thumbnail[kThumbLengthV2];
  union {
    be<uint16_t> uint[kNumLanguagesV2 - kNumLanguagesV1][128];
    char16_t chars[kNumLanguagesV2 - kNumLanguagesV1][128];
  } description_ex_raw;

  std::u16string display_name(XLanguage language) const {
    uint32_t lang_id = uint32_t(language) - 1;

    if (lang_id >= kNumLanguagesV2) {
      assert_always();
      // no room for this lang, read from english slot..
      lang_id = uint32_t(XLanguage::kEnglish) - 1;
    }

    const be<uint16_t>* str = 0;
    if (lang_id >= 0 && lang_id < kNumLanguagesV1) {
      str = display_name_raw.uint[lang_id];
    } else if (lang_id >= kNumLanguagesV1 && lang_id < kNumLanguagesV2 &&
               metadata_version >= 2) {
      str = display_name_ex_raw.uint[lang_id - kNumLanguagesV1];
    }

    if (!str) {
      // Invalid language ID?
      assert_always();
      return u"";
    }

    return load_and_swap<std::u16string>(str);
  }

  std::u16string description(XLanguage language) const {
    uint32_t lang_id = uint32_t(language) - 1;

    if (lang_id >= kNumLanguagesV2) {
      assert_always();
      // no room for this lang, read from english slot..
      lang_id = uint32_t(XLanguage::kEnglish) - 1;
    }

    const be<uint16_t>* str = 0;
    if (lang_id >= 0 && lang_id < kNumLanguagesV1) {
      str = description_raw.uint[lang_id];
    } else if (lang_id >= kNumLanguagesV1 && lang_id < kNumLanguagesV2 &&
               metadata_version >= 2) {
      str = description_ex_raw.uint[lang_id - kNumLanguagesV1];
    }

    if (!str) {
      // Invalid language ID?
      assert_always();
      return u"";
    }

    return load_and_swap<std::u16string>(str);
  }

  std::u16string publisher() const {
    return load_and_swap<std::u16string>(publisher_raw.uint);
  }

  std::u16string title_name() const {
    return load_and_swap<std::u16string>(title_name_raw.uint);
  }

  bool set_display_name(XLanguage language, const std::u16string_view value) {
    uint32_t lang_id = uint32_t(language) - 1;

    if (lang_id >= kNumLanguagesV2) {
      assert_always();
      // no room for this lang, store in english slot..
      lang_id = uint32_t(XLanguage::kEnglish) - 1;
    }

    char16_t* str = 0;
    if (lang_id >= 0 && lang_id < kNumLanguagesV1) {
      str = display_name_raw.chars[lang_id];
    } else if (lang_id >= kNumLanguagesV1 && lang_id < kNumLanguagesV2 &&
               metadata_version >= 2) {
      str = display_name_ex_raw.chars[lang_id - kNumLanguagesV1];
    }

    if (!str) {
      // Invalid language ID?
      assert_always();
      return false;
    }

    string_util::copy_and_swap_truncating(str, value,
                                          countof(display_name_raw.chars[0]));
    return true;
  }

  bool set_description(XLanguage language, const std::u16string_view value) {
    uint32_t lang_id = uint32_t(language) - 1;

    if (lang_id >= kNumLanguagesV2) {
      assert_always();
      // no room for this lang, store in english slot..
      lang_id = uint32_t(XLanguage::kEnglish) - 1;
    }

    char16_t* str = 0;
    if (lang_id >= 0 && lang_id < kNumLanguagesV1) {
      str = description_raw.chars[lang_id];
    } else if (lang_id >= kNumLanguagesV1 && lang_id < kNumLanguagesV2 &&
               metadata_version >= 2) {
      str = description_ex_raw.chars[lang_id - kNumLanguagesV1];
    }

    if (!str) {
      // Invalid language ID?
      assert_always();
      return false;
    }

    string_util::copy_and_swap_truncating(str, value,
                                          countof(description_raw.chars[0]));
    return true;
  }

  void set_publisher(const std::u16string_view value) {
    string_util::copy_and_swap_truncating(publisher_raw.chars, value,
                                          countof(publisher_raw.chars));
  }

  void set_title_name(const std::u16string_view value) {
    string_util::copy_and_swap_truncating(title_name_raw.chars, value,
                                          countof(title_name_raw.chars));
  }
};
static_assert_size(XContentMetadata, 0x93D6);

struct XContentHeader {
  be<XContentPackageType> magic;
  union {
    // signature used by LIVE/PIRS content packages
    uint8_t online[0x228];
    // signature used by CON (console-signed) savegame/profile packages
    X_XE_CONSOLE_SIGNATURE console;
  } signature;

  XContentLicense licenses[0x10];
  uint8_t content_id[0x14];
  be<uint32_t> header_size;

  bool is_magic_valid() const {
    return magic == XContentPackageType::kCon ||
           magic == XContentPackageType::kLive ||
           magic == XContentPackageType::kPirs;
  }
};
static_assert_size(XContentHeader, 0x344);
#pragma pack(pop)

struct XContentContainerHeader {
  XContentHeader content_header;
  XContentMetadata content_metadata;
  // TODO: title/system updates contain more data after XContentMetadata, seems
  // to affect header.header_size

  bool is_package_readonly() const {
    if (content_metadata.volume_type == XContentVolumeType::kSvod) {
      return true;
    }

    return content_metadata.volume_descriptor.stfs.flags.bits.read_only_format;
  }
};
static_assert_size(XContentContainerHeader, 0x971A);

struct XCONTENT_DATA {
  be<uint32_t> device_id;
  be<XContentType> content_type;
  union {
    // this should be be<uint16_t>, but that stops copy constructor being
    // generated...
    uint16_t uint[128];
    char16_t chars[128];
  } display_name_raw;

  char file_name_raw[42];

  // Some games use this padding field as a null-terminator, as eg.
  // DLC packages usually fill the entire file_name_raw array
  // Not every game sets it to 0 though, so make sure any file_name_raw reads
  // only go up to 42 chars!
  uint8_t padding[2];

  bool operator==(const XCONTENT_DATA& other) const {
    // Package is located via device_id/content_type/file_name, so only need to
    // compare those
    return device_id == other.device_id && content_type == other.content_type &&
           file_name() == other.file_name();
  }

  std::u16string display_name() const {
    return load_and_swap<std::u16string>(display_name_raw.uint);
  }

  std::string file_name() const {
    std::string value;
    value.assign(file_name_raw,
                 std::min(strlen(file_name_raw), countof(file_name_raw)));
    return value;
  }

  void set_display_name(const std::u16string_view value) {
    // Some games (e.g. 584108A9) require multiple null-terminators for it to
    // read the string properly, blanking the array should take care of that

    std::fill_n(display_name_raw.chars, countof(display_name_raw.chars), 0);
    string_util::copy_and_swap_truncating(display_name_raw.chars, value,
                                          countof(display_name_raw.chars));
  }

  void set_file_name(const std::string_view value) {
    std::fill_n(file_name_raw, countof(file_name_raw), 0);
    string_util::copy_maybe_truncating<string_util::Safety::IKnowWhatIAmDoing>(
        file_name_raw, value, xe::countof(file_name_raw));

    // Some games rely on padding field acting as a null-terminator...
    padding[0] = padding[1] = 0;
  }
};
static_assert_size(XCONTENT_DATA, 0x134);

struct XCONTENT_AGGREGATE_DATA : XCONTENT_DATA {
  be<uint64_t> xuid;
  be<uint32_t> title_id;

  XCONTENT_AGGREGATE_DATA() = default;
  XCONTENT_AGGREGATE_DATA(const XCONTENT_DATA& other) {
    device_id = other.device_id;
    content_type = other.content_type;
    set_display_name(other.display_name());
    set_file_name(other.file_name());
    padding[0] = padding[1] = 0;
    xuid = 0;
    title_id = kCurrentlyRunningTitleId;
  }

  bool operator==(const XCONTENT_AGGREGATE_DATA& other) const {
    // Package is located via device_id/title_id/content_type/file_name, so only
    // need to compare those
    return device_id == other.device_id && title_id == other.title_id &&
           content_type == other.content_type &&
           file_name() == other.file_name();
  }
};
static_assert_size(XCONTENT_AGGREGATE_DATA, 0x148);

}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_XCONTENT_H_