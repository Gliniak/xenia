/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <filesystem>

#include "xenia/avatars/animation.h"
#include "xenia/avatars/asset_pack.h"
#include "xenia/avatars/compression.h"
#include "xenia/avatars/guest_animation.h"
#include "xenia/avatars/guest_load_animation.h"
#include "xenia/avatars/strb.h"
#include "xenia/base/bit_stream.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"

namespace xe {
namespace avatars {

struct X_AVATAR_VECTOR_SERIALIZER {
  be<float> quant_radius;
  be<float> delta_x;
  be<float> delta_y;
  be<float> delta_z;
  be<float> base_x;
  be<float> base_y;
  be<float> base_z;
  uint8_t unknown[12];
  be<uint32_t> bit_count_x;
  be<uint32_t> bit_count_y;
  be<uint32_t> bit_count_z;
};
static_assert_size(X_AVATAR_VECTOR_SERIALIZER, 0x34);

struct X_AVATAR_QUATERNION_SERIALIZER {
  X_AVATAR_VECTOR_SERIALIZER base_serializer;
};
static_assert_size(X_AVATAR_QUATERNION_SERIALIZER, 0x34);

struct X_AVATAR_DWORD_SERIALIZER {
  be<uint32_t> bit_count;
  be<uint32_t> base_value;
  uint8_t unknown[4];
};
static_assert_size(X_AVATAR_DWORD_SERIALIZER, 0xC);

struct X_AVATAR_JOINT_SERIALIZER {
  X_AVATAR_VECTOR_SERIALIZER position_serializer;
  X_AVATAR_QUATERNION_SERIALIZER rotation_serializer;
  X_AVATAR_VECTOR_SERIALIZER scale_serializer;
};
static_assert_size(X_AVATAR_JOINT_SERIALIZER, 0x9C);

struct X_AVATAR_MOTION_SERIALIZER {
  X_AVATAR_VECTOR_SERIALIZER position_serializer;
  X_AVATAR_QUATERNION_SERIALIZER rotation_serializer;
};
static_assert_size(X_AVATAR_MOTION_SERIALIZER, 0x68);

struct X_AVATAR_TEXTURE_SERIALIZER {
  X_AVATAR_DWORD_SERIALIZER layer_index_serializer;
};
static_assert_size(X_AVATAR_TEXTURE_SERIALIZER, 0xC);

struct X_AVATAR_POSE_FRAME_SET {
  be<uint32_t> frame_count;
  be<uint32_t> element_count;
  be<uint32_t> frame_bit_count;
  X_AVATAR_JOINT_SERIALIZER element_serializers[72];
};
static_assert_size(X_AVATAR_POSE_FRAME_SET, 0x2BEC);

struct X_AVATAR_MOTION_FRAME_SET {
  be<uint32_t> frame_count;
  be<uint32_t> element_count;
  be<uint32_t> frame_bit_count;
  X_AVATAR_MOTION_SERIALIZER element_serializers[3];
};
static_assert_size(X_AVATAR_MOTION_FRAME_SET, 0x144);

struct X_AVATAR_TEXTURE_FRAME_SET {
  be<uint32_t> frame_count;
  be<uint32_t> element_count;
  be<uint32_t> frame_bit_count;
  X_AVATAR_TEXTURE_SERIALIZER element_serializers[5];
};
static_assert_size(X_AVATAR_TEXTURE_FRAME_SET, 0x48);

struct X_AVATAR_ANIMATION {
  X_AVATAR_POSE_FRAME_SET pose_frame_sets[2];
  X_AVATAR_MOTION_FRAME_SET motion_frame_set;
  X_AVATAR_TEXTURE_FRAME_SET texture_frame_set;
  be<uint32_t> frame_count;
  be<float> frames_per_second;
  be<uint32_t> pose_counts[2];
  be<uint32_t> motion_count;
  be<uint32_t> texture_count;
  be<uint32_t> pose_2_offset;
  be<uint32_t> motions_offset;
  be<uint32_t> textures_offset;
  be<uint32_t> compressed_data_size;
  be<uint32_t> compressed_data_buffer_ptr;
};
static_assert_size(X_AVATAR_ANIMATION, 0x5990);

void VectorSerializerToGuest(const VectorSerializer& host_serializer,
                             X_AVATAR_VECTOR_SERIALIZER& guest_serializer) {
  guest_serializer.quant_radius = host_serializer.quant_radius;
  guest_serializer.delta_x = host_serializer.delta_x;
  guest_serializer.delta_y = host_serializer.delta_y;
  guest_serializer.delta_z = host_serializer.delta_z;
  guest_serializer.base_x = host_serializer.base_x;
  guest_serializer.base_y = host_serializer.base_y;
  guest_serializer.base_z = host_serializer.base_z;
  // unknown[12]
  std::memset(guest_serializer.unknown, 0, sizeof(guest_serializer.unknown));
  guest_serializer.bit_count_x = host_serializer.bit_count_x;
  guest_serializer.bit_count_y = host_serializer.bit_count_y;
  guest_serializer.bit_count_z = host_serializer.bit_count_z;
}

void QuaternionSerializerToGuest(
    const QuaternionSerializer& host_serializer,
    X_AVATAR_QUATERNION_SERIALIZER& guest_serializer) {
  VectorSerializerToGuest(host_serializer.base_serializer,
                          guest_serializer.base_serializer);
}

void DwordSerializerToGuest(const ValueSerializer<uint32_t>& host_serializer,
                            X_AVATAR_DWORD_SERIALIZER& guest_serializer) {
  guest_serializer.bit_count = host_serializer.bit_count;
  guest_serializer.base_value = host_serializer.base_value;
  // unknown[4]
  std::memset(guest_serializer.unknown, 0, sizeof(guest_serializer.unknown));
}

void PoseFrameSetToGuest(const Animation::PoseFrameSet& host,
                         X_AVATAR_POSE_FRAME_SET& guest, size_t element_count) {
  guest.frame_count = static_cast<uint32_t>(host.frame_count);
  guest.element_count = static_cast<uint32_t>(element_count);
  guest.frame_bit_count = static_cast<uint32_t>(host.frame_bit_count);
  assert_true(countof(guest.element_serializers) ==
              countof(host.element_serializers));
  for (size_t i = 0; i < countof(guest.element_serializers); ++i) {
    const auto& host_element_serializer = host.element_serializers[i];
    auto& guest_element_serializer = guest.element_serializers[i];
    VectorSerializerToGuest(host_element_serializer.position_serializer,
                            guest_element_serializer.position_serializer);
    QuaternionSerializerToGuest(host_element_serializer.rotation_serializer,
                                guest_element_serializer.rotation_serializer);
    VectorSerializerToGuest(host_element_serializer.scale_serializer,
                            guest_element_serializer.scale_serializer);
  }
}

void MotionFrameSetToGuest(const Animation::MotionFrameSet& host,
                           X_AVATAR_MOTION_FRAME_SET& guest,
                           size_t element_count) {
  guest.frame_count = static_cast<uint32_t>(host.frame_count);
  guest.element_count = static_cast<uint32_t>(element_count);
  guest.frame_bit_count = static_cast<uint32_t>(host.frame_bit_count);
  assert_true(countof(guest.element_serializers) ==
              countof(host.element_serializers));
  for (size_t i = 0; i < countof(guest.element_serializers); ++i) {
    const auto& host_element_serializer = host.element_serializers[i];
    auto& guest_element_serializer = guest.element_serializers[i];
    VectorSerializerToGuest(host_element_serializer.position_serializer,
                            guest_element_serializer.position_serializer);
    QuaternionSerializerToGuest(host_element_serializer.rotation_serializer,
                                guest_element_serializer.rotation_serializer);
  }
}

void TextureFrameSetToGuest(const Animation::TextureFrameSet& host,
                            X_AVATAR_TEXTURE_FRAME_SET& guest,
                            size_t element_count) {
  guest.frame_count = static_cast<uint32_t>(host.frame_count);
  guest.element_count = static_cast<uint32_t>(element_count);
  guest.frame_bit_count = static_cast<uint32_t>(host.frame_bit_count);
  assert_true(countof(guest.element_serializers) ==
              countof(host.element_serializers));
  for (size_t i = 0; i < countof(guest.element_serializers); ++i) {
    const auto& host_element_serializer = host.element_serializers[i];
    auto& guest_element_serializer = guest.element_serializers[i];
    DwordSerializerToGuest(host_element_serializer.layer_index_serializer,
                           guest_element_serializer.layer_index_serializer);
  }
}

bool LoadAnimationToGuest(const AssetId& asset_id,
                          kernel::KernelState* kernel_state,
                          X_AVATAR_ANIMATION* guest_animation,
                          uint32_t coordinate_system) {
  const uint8_t* strb_buffer;
  size_t strb_size;
  if (!kernel_state->avatar_asset_pack()->GetAssetData(asset_id, strb_buffer,
                                                       strb_size)) {
    XELOGE("Failed to find avatar animation {}!", asset_id.to_string());
    return false;
  }

  /*
  auto dump2_name = fmt::format("animation_{}.bin", asset_id.to_string());
  auto dump2_handle = fopen(dump2_name.c_str(), "wb");
  fwrite(strb_buffer, 1, strb_size, dump2_handle);
  fclose(dump2_handle);
  */

  AnimationLoadOptions load_options = AnimationLoadOption::kGuest;
  if (coordinate_system == 0) {
    load_options |= AnimationLoadOption::kInvert;
  }

  auto animation = Animation::Load(strb_buffer, strb_size, load_options);
  if (!animation) {
    XELOGE("Failed to load avatar animation {}!", asset_id.to_string());
    return false;
  }

  if (animation->compressed_data_bytes.size() > guest_animation->compressed_data_size) {
    XELOGE(
        "Not enough space to copy avatar animation compressed data for {}! ({} "
        "> {})",
        asset_id.to_string(), animation->compressed_data_bytes.size(),
        guest_animation->compressed_data_size.get());
    return false;
  }

  assert_true(countof(guest_animation->pose_frame_sets) ==
              countof(animation->pose_frame_sets));
  for (size_t i = 0; i < countof(guest_animation->pose_frame_sets); ++i) {
    const auto& pose_frame_set = animation->pose_frame_sets[i];
    auto& guest_pose_frame_set = guest_animation->pose_frame_sets[i];
    PoseFrameSetToGuest(pose_frame_set, guest_pose_frame_set,
                        animation->pose_counts[i]);
  }

  MotionFrameSetToGuest(animation->motion_frame_set,
                        guest_animation->motion_frame_set,
                        animation->motion_count);

  TextureFrameSetToGuest(animation->texture_frame_set,
                         guest_animation->texture_frame_set,
                         animation->texture_count);

  assert_true(offsetof(X_AVATAR_ANIMATION, frame_count) == 0x5964);
  assert_true(offsetof(X_AVATAR_ANIMATION, frames_per_second) == 0x5968);
  assert_true(offsetof(X_AVATAR_ANIMATION, pose_counts) == 0x596C);
  assert_true(offsetof(X_AVATAR_ANIMATION, motion_count) == 0x5974);
  assert_true(offsetof(X_AVATAR_ANIMATION, texture_count) == 0x5978);
  assert_true(offsetof(X_AVATAR_ANIMATION, pose_2_offset) == 0x597C);
  assert_true(offsetof(X_AVATAR_ANIMATION, motions_offset) == 0x5980);
  assert_true(offsetof(X_AVATAR_ANIMATION, textures_offset) == 0x5984);
  assert_true(offsetof(X_AVATAR_ANIMATION, compressed_data_size) == 0x5988);
  assert_true(offsetof(X_AVATAR_ANIMATION, compressed_data_buffer_ptr) == 0x598C);

  guest_animation->frame_count = animation->frame_count;
  guest_animation->frames_per_second = animation->frames_per_second;
  guest_animation->pose_counts[0] = animation->pose_counts[0];
  guest_animation->pose_counts[1] = animation->pose_counts[1];
  guest_animation->motion_count = animation->motion_count;
  guest_animation->texture_count = animation->texture_count;
  guest_animation->pose_2_offset = animation->pose_2_byte_offset;
  guest_animation->motions_offset = animation->motions_byte_offset;
  guest_animation->textures_offset = animation->textures_byte_offset;
  guest_animation->compressed_data_size =
      static_cast<uint32_t>(animation->compressed_data_bytes.size());

  auto guest_compressed_buffer = kernel_state->memory()->TranslateVirtual(
      guest_animation->compressed_data_buffer_ptr);
  std::memcpy(guest_compressed_buffer, animation->compressed_data_bytes.data(),
              guest_animation->compressed_data_size);

  /*
  auto dump_name = fmt::format("animation_{}_memory_xenia.bin", asset_id.to_string());
  auto dump_handle = fopen(dump_name.c_str(), "wb");
  fwrite(guest_animation, sizeof(X_AVATAR_ANIMATION), 1, dump_handle);
  fclose(dump_handle);
  */

  return true;
}

bool LoadAnimationToGuest(const AssetId& asset_id,
                          kernel::KernelState* kernel_state,
                          void* guest_animation, uint32_t coordinate_system) {
  return LoadAnimationToGuest(asset_id, kernel_state,
                              static_cast<X_AVATAR_ANIMATION*>(guest_animation),
                              coordinate_system);
}

}  // namespace avatars
}  // namespace xe
