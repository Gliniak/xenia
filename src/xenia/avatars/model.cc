/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/avatars/model.h"

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

ShaderParameter ShaderParameter::Load(BitStream& stream) {
  ShaderParameter instance = {};
  instance.type = static_cast<ShaderParameterType>(stream.Read<uint32_t>());
  instance.usage = stream.Read<uint32_t>();
  if (instance.type == ShaderParameterType::kTexture) {
    instance.texture.index = stream.Read<uint16_t>();
    instance.texture.uv_layer = stream.Read<uint16_t>();
    instance.texture.flags = stream.Read<uint32_t>();
    stream.Advance(sizeof(uint32_t) * 8 * 2);
  } else {
    for (size_t i = 0; i < countof(instance.constant_values); ++i) {
      instance.constant_values[i] = stream.Read<float>();
    }
  }
  return instance;
}

TriangleBatch TriangleBatch::Read(BitStream& stream,
                                  ModelLoadOptions load_options) {
  TriangleBatch instance;
  instance.shader_id = stream.Read<uint32_t>();
  uint8_t shader_parameter_count = stream.Read<uint8_t>(5);
  instance.triangle_count = stream.Read<uint32_t>();
  uint32_t vertex_count = stream.Read<uint32_t>();
  instance.uv_count = stream.Read<uint32_t>();
  instance.vertex_size = stream.Read<uint32_t>();
  instance.index_size = stream.Read<uint32_t>();
  instance.vertex_array_offset = stream.Read<uint32_t>();
  instance.index_array_offset = stream.Read<uint32_t>();
  stream.AlignToNextByte();

  assert_true(instance.triangle_count <= 8192);
  assert_true(vertex_count <= 8192);
  assert_true(instance.uv_count <= 6);
  assert_true(instance.vertex_size == 32u + 4u * (instance.uv_count - 1u));

  for (uint8_t i = 0; i < shader_parameter_count; ++i) {
    instance.shader_parameters.push_back(ShaderParameter::Load(stream));
    stream.AlignToNextByte();
  }

  instance.vertices =
      ReadVertices(stream, vertex_count, instance.uv_count, load_options);
  stream.AlignToNextByte();

  instance.indices = ReadIndices(stream, instance.triangle_count);
  stream.AlignToNextByte();

  return instance;
}

std::vector<Vertex> TriangleBatch::ReadVertices(BitStream& stream,
                                                size_t vertex_count,
                                                size_t uv_count,
                                                ModelLoadOptions load_options) {
  uint32_t local_vertex_count = stream.Read<uint32_t>();
  assert_true(vertex_count == local_vertex_count);

  auto vector_serializer = VectorSerializer::From(stream);
  auto normal_serializer = ValueSerializer<int32_t>::From(stream);
  auto blend_weight_serializer = ValueSerializer<uint32_t>::From(stream);
  auto blend_indices_serializer = ValueSerializer<uint32_t>::From(stream);
  auto color_serializer = ValueSerializer<uint32_t>::From(stream);

  if (load_options & ModelLoadOption::kInvert) {
    vector_serializer.invert();
  }

  std::vector<Vertex> vertices;
  for (size_t i = 0; i < vertex_count; ++i) {
    Vertex vertex;
    vertex.position = vector_serializer.Read(stream);
    vertex.normal = normal_serializer.Read(stream);
    if (load_options & ModelLoadOption::kInvert) {
      uint32_t normal_z = vertex.normal >> 22;
      normal_z = ~normal_z + 1;
      vertex.normal &= ~(0x3FFu << 22);
      vertex.normal |= normal_z << 22;
    }
    vertex.blend_weight = blend_weight_serializer.Read(stream);
    vertex.blend_indices = blend_indices_serializer.Read(stream);
    vertex.color = color_serializer.Read(stream);
    for (size_t j = 0; j < uv_count; ++j) {
      Vector2<uint16_t> uv;
      uv.x = stream.Read<uint16_t>();
      uv.y = stream.Read<uint16_t>();
      vertex.uvs.push_back(uv);
    }
    vertices.push_back(vertex);
  }
  return vertices;
}

std::vector<uint16_t> TriangleBatch::ReadIndices(BitStream& stream,
                                                 size_t triangle_count) {
  uint32_t index_count = stream.Read<uint32_t>();
  assert_true(index_count == triangle_count * 3);

  auto index_serializer = ValueSerializer<uint16_t>::From(stream);

  std::vector<uint16_t> indices;
  for (size_t i = 0; i < index_count; ++i) {
    indices.push_back(index_serializer.Read(stream));
  }
  return indices;
}

Texture Texture::Read(BitStream& stream) {
  Texture instance;
  instance.format = stream.Read<uint32_t>();
  instance.width = stream.Read<uint32_t>();
  instance.height = stream.Read<uint32_t>();
  instance.total_data_size = stream.Read<uint32_t>();
  instance.data_size = stream.Read<uint32_t>();
  instance.layer_count = stream.Read<uint32_t>();
  instance.is_empty = stream.Read<bool>();
  instance.is_tiled = stream.Read<bool>();
  instance.data_stride = stream.Read<uint32_t>();
  instance.data_rows = stream.Read<uint32_t>();
  stream.AlignToNextByte();

  assert_true(instance.width <= 1024);
  assert_true(instance.height <= 1024);
  assert_true(instance.layer_count >= 1 && instance.layer_count <= 14);

  if (!instance.is_empty) {
    size_t data_size = instance.data_stride;
    data_size *= instance.data_rows;
    data_size *= instance.layer_count;
    size_t data_bit_size = data_size * 8;
    assert_true(stream.offset_bits() + data_bit_size <= stream.size_bits());
    instance.data_bytes.resize(data_size);
    std::memcpy(instance.data_bytes.data(),
                &stream.buffer()[stream.offset_bits() >> 3], data_size);
    stream.Advance(data_bit_size);
  }

  return instance;
}

std::shared_ptr<Texture> Texture::Read(const uint8_t* data_buffer,
                                       size_t data_size) {
  BitStream stream(data_buffer, data_size * 8);

  auto instance = Read(stream);

  assert_true(stream.offset_bits() == stream.size_bits());

  return std::make_shared<Texture>(instance);
}

std::shared_ptr<Texture> Texture::Load(const uint8_t* strb_buffer,
                                       size_t strb_size) {
  const uint8_t* compressed_buffer;
  size_t compressed_size;
  if (!strb::GetSTRBBlock(strb_buffer, strb_size, strb::STRBBlockId::kTexture,
                          compressed_buffer, compressed_size)) {
    return nullptr;
  }

  size_t data_size;
  if (!compression::GetUncompressedSize(compressed_buffer, compressed_size,
                                        data_size)) {
    assert_always();
    XELOGE("Failed to get uncompressed size for avatar texture!");
    return nullptr;
  }

  std::vector<uint8_t> data_bytes(data_size);
  if (!compression::Decompress(compressed_buffer, compressed_size,
                               data_bytes.data(), data_size)) {
    assert_always();
    XELOGE("Failed to decompress avatar texture!");
    return nullptr;
  }

  return Read(data_bytes.data(), data_bytes.size());
}

ModelTexture ModelTexture::Read(BitStream& stream) {
  ModelTexture instance;
  instance.gpu_offset = stream.Read<uint32_t>();
  instance.gpu_size = stream.Read<uint32_t>();
  instance.texture = Texture::Read(stream);
  return instance;
}

std::shared_ptr<Model> Model::Read(const uint8_t* data_buffer, size_t data_size,
                                   ModelLoadOptions load_options) {
  BitStream stream(data_buffer, data_size * 8);

  auto instance = std::make_shared<Model>();

  instance->cpu_size = stream.Read<int32_t>();
  instance->gpu_size = stream.Read<int32_t>();
  instance->texture_buffer_size = stream.Read<int32_t>();
  instance->vertex_buffer_size = stream.Read<int32_t>();
  instance->index_buffer_size = stream.Read<int32_t>();
  uint32_t triangle_batch_count = stream.Read<uint32_t>();
  uint32_t texture_count = stream.Read<uint32_t>();
  instance->vertex_buffer_offset = stream.Read<uint32_t>();
  instance->index_buffer_offset = stream.Read<int32_t>();
  instance->triangle_batch_array_offset = stream.Read<int32_t>();
  instance->texture_array_offset = stream.Read<int32_t>();
  instance->texture_scratch_size = stream.Read<int32_t>();
  assert_true(triangle_batch_count <= 16);
  assert_true(texture_count <= 18);

  for (uint32_t i = 0; i < triangle_batch_count; ++i) {
    instance->triangle_batches.push_back(
        TriangleBatch::Read(stream, load_options));
    stream.AlignToNextByte();
  }

  for (uint32_t i = 0; i < texture_count; ++i) {
    instance->textures.push_back(ModelTexture::Read(stream));
    stream.AlignToNextByte();
  }

  for (;;) {
    auto vertex_pair_count = stream.Read<uint32_t>();
    assert_true(vertex_pair_count <= 400);
    auto vertex_pair_serializer = ValueSerializer<uint32_t>::From(stream);
    for (uint32_t i = 0; i < vertex_pair_count; ++i) {
      auto vertex_pair_gpu_offset = vertex_pair_serializer.Read(stream);
    }
    stream.AlignToNextByte();
    if ((vertex_pair_count & 1) != 0) {
      break;
    }
  }

  assert_true(stream.offset_bits() == stream.size_bits());

  return instance;
}

std::shared_ptr<Model> Model::Load(const uint8_t* strb_buffer, size_t strb_size,
                                   ModelLoadOptions load_options) {
  const uint8_t* compressed_buffer;
  size_t compressed_size;
  if (!strb::GetSTRBBlock(strb_buffer, strb_size, strb::STRBBlockId::kModel,
                          compressed_buffer, compressed_size)) {
    return nullptr;
  }

  size_t data_size;
  if (!compression::GetUncompressedSize(compressed_buffer, compressed_size,
                                        data_size)) {
    assert_always();
    XELOGE("Failed to get uncompressed size for avatar model!");
    return nullptr;
  }

  std::vector<uint8_t> data_bytes(data_size);
  if (!compression::Decompress(compressed_buffer, compressed_size,
                               data_bytes.data(), data_size)) {
    assert_always();
    XELOGE("Failed to decompress avatar model!");
    return nullptr;
  }

  return Read(data_bytes.data(), data_bytes.size(), load_options);
}

}  // namespace avatars
}  // namespace xe
