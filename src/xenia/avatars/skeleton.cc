/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
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

#include <DirectXMath.h>

namespace xe {
namespace avatars {

void Skeleton::Initialize() {
  const uint8_t InvalidIndex = 255;

  assert_true(joints.size() < 255);

  uint8_t jointCount = static_cast<uint8_t>(joints.size());

  for (uint8_t i = 0; i < jointCount; ++i) {
    auto& joint = joints[i];
    joint.first_child_index = InvalidIndex;
    joint.next_index = InvalidIndex;
    joint.pose.position = joint.bindpose.position;
    joint.pose.rotation = joint.bindpose.rotation;
    joint.pose.scale = {1.0f, 1.0f, 1.0f};
  }

  for (uint8_t i = jointCount - 1; i >= 1; --i) {
    auto& joint = joints[i];
    const auto& parentJoint = joints[joint.parent_index];

    auto matrixA = DirectX::XMMatrixRotationQuaternion(DirectX::XMVectorSet(
        joint.bindpose.rotation.x, joint.bindpose.rotation.y,
        joint.bindpose.rotation.z, 1.f));
    matrixA.r[3] = DirectX::XMVectorSet(joint.bindpose.position.x,
                                        joint.bindpose.position.y,
                                        joint.bindpose.position.z, 1.f);

    auto matrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMVectorSet(
        parentJoint.bindpose.rotation.x, parentJoint.bindpose.rotation.y,
        parentJoint.bindpose.rotation.z, 1.f));
    matrix.r[3] = DirectX::XMVectorSet(parentJoint.bindpose.position.x,
                                       parentJoint.bindpose.position.y,
                                       parentJoint.bindpose.position.z, 1.f);

    auto matrixB = DirectX::XMMatrixInverse(nullptr, matrix);
    auto matrix2 = DirectX::XMMatrixMultiply(matrixA, matrixB);

    auto position = matrix2.r[3];
    auto rotation = DirectX::XMQuaternionRotationMatrix(matrix2);

    joint.pose.position.x = position.m128_f32[0];
    joint.pose.position.y = position.m128_f32[1];
    joint.pose.position.z = position.m128_f32[2];
    // joint.pose.position.w = position.m128_f32[3];
    joint.pose.rotation.x = rotation.m128_f32[0];
    joint.pose.rotation.y = rotation.m128_f32[1];
    joint.pose.rotation.z = rotation.m128_f32[2];
    joint.pose.rotation.w = rotation.m128_f32[3];
  }

  {
    auto& joint = joints[0];
    joint.pose.position = joint.bindpose.position;
    // joint.pose.position.w = 1.f;
    joint.pose.rotation = joint.bindpose.rotation;
  }

  for (uint8_t parentIndex = 0; parentIndex < jointCount; ++parentIndex) {
    uint8_t firstChildIndex = InvalidIndex;
    for (uint8_t childIndex = 0; childIndex < jointCount; ++childIndex) {
      if (joints[childIndex].parent_index == parentIndex) {
        firstChildIndex = childIndex;
        break;
      }
    }

    // didn't find any children
    if (firstChildIndex == InvalidIndex) {
      continue;
    }

    joints[parentIndex].first_child_index = firstChildIndex;

    // find next
    uint8_t prevIndex = firstChildIndex;
    for (uint8_t nextIndex = firstChildIndex + 1; nextIndex < jointCount;
         ++nextIndex) {
      if (joints[nextIndex].parent_index == parentIndex) {
        joints[prevIndex].next_index = nextIndex;
        prevIndex = nextIndex;
      }
    }
  }
}

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

  instance->Initialize();

  return instance;
}

std::shared_ptr<Skeleton> Skeleton::Load(const uint8_t* strb_buffer,
                                         size_t strb_size,
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
