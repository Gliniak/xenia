/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_AVATARS_SKELETON_H_
#define XENIA_AVATARS_SKELETON_H_

#include <memory>
#include <vector>

#include "xenia/avatars/common.h"
#include "xenia/avatars/serializers.h"
#include "xenia/base/memory.h"

namespace xe {
namespace avatars {

class BitStream;

struct JointBindPose {
  Vector3<float> position;
  Quaternion<float> rotation;
};

struct JointPose {
  Vector3<float> position;
  Quaternion<float> rotation;
  Vector3<float> scale;
};

struct Joint {
  uint8_t parent_index;
  uint8_t first_child_index;
  uint8_t next_index;
  JointBindPose bindpose;
  JointPose pose;
};

struct JointSerializer {
 public:
  VectorSerializer position_serializer;
  QuaternionSerializer rotation_serializer;

  size_t element_bit_size() const {
    return 8 + position_serializer.element_bit_size() +
           rotation_serializer.element_bit_size();
  }

 public:
  static JointSerializer From(BitStream& stream) {
    JointSerializer instance;
    instance.position_serializer = VectorSerializer::From(stream);
    instance.rotation_serializer = QuaternionSerializer::From(stream);
    return instance;
  }

  void invert() {
    position_serializer.invert();
    rotation_serializer.invert();
  }

  Joint Read(BitStream& stream) const {
    Joint instance;
    instance.parent_index = stream.Read<uint8_t>();
    instance.bindpose.position = position_serializer.Read(stream);
    instance.bindpose.rotation = rotation_serializer.Read(stream);
    return instance;
  }
};

typedef uint32_t SkeletonLoadOptions;

namespace SkeletonLoadOption {

using Option = SkeletonLoadOptions;

const Option kNone = 0;

const Option kInvert = 1 << 0;

}  // namespace SkeletonLoadOption

class Skeleton {
 public:
  std::vector<Joint> joints;

 private:
  void Initialize();

 public:
  static std::shared_ptr<Skeleton> Load(const uint8_t* strb_buffer,
                                        size_t strb_size,
                                        SkeletonLoadOptions load_options);

  static std::shared_ptr<Skeleton> Read(const uint8_t* data_buffer,
                                        size_t data_size,
                                        SkeletonLoadOptions load_options);
};

}  // namespace avatars
}  // namespace xe

#endif  // XENIA_AVATARS_SKELETON_H_
