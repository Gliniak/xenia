/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/avatars/skeleton.h"

#include "xenia/avatars/bit_stream.h"
#include "xenia/avatars/common.h"
#include "xenia/avatars/compression.h"
#include "xenia/avatars/serializers.h"
#include "xenia/avatars/strb.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/kernel/kernel_state.h"

namespace xe {
namespace avatars {

std::shared_ptr<Skeleton> Skeleton::Read(const uint8_t* data_buffer,
                                         size_t data_size,
                                         SkeletonLoadOptions load_options) {
  BitStream stream(data_buffer, data_size * 8);

  auto instance = std::make_shared<Skeleton>();

  size_t joint_count = stream.Read<uint32_t>();
  assert_true(joint_count <= 72);

  auto joint_serializer = JointSerializer::From(stream);
  if (load_options & SkeletonLoadOption::kInvert) {
    joint_serializer.invert();
  }
  for (size_t i = 0; i < joint_count; ++i) {
    auto joint = joint_serializer.Read(stream);
    instance->joints.push_back(joint);
  }

  stream.AlignToNextByte();
  assert_true(stream.offset_bits() == stream.size_bits());

  return instance;
}

std::shared_ptr<Skeleton> Skeleton::ReadFromStrb(
    const uint8_t* strb_buffer, size_t strb_size,
    SkeletonLoadOptions load_options) {
  const uint8_t* data_buffer;
  size_t data_size;
  if (!strb::GetSTRBBlock(strb_buffer, strb_size, strb::STRBBlockId::kSkeleton,
                          data_buffer, data_size)) {
    return nullptr;
  }
  return Read(data_buffer, data_size, load_options);
}

}  // namespace avatars
}  // namespace xe
