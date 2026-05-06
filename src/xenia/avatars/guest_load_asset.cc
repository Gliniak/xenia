/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <filesystem>
#include <stack>

#include "xenia/avatars/asset_pack.h"
#include "xenia/avatars/compression.h"
#include "xenia/avatars/guest_asset.h"
#include "xenia/avatars/guest_load_asset.h"
#include "xenia/avatars/memory_block.h"
#include "xenia/avatars/model.h"
#include "xenia/avatars/skeleton.h"
#include "xenia/avatars/skeleton_scaling.h"
#include "xenia/avatars/strb.h"
#include "xenia/base/bit_stream.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"

namespace xe {
namespace avatars {

static uint8_t skeleton_nxe[] = {
    0x47, 0x00, 0x00, 0x00, 0x02, 0x44, 0xED, 0x36, 0x78, 0xD6, 0x24, 0xBF,
    0x19, 0x66, 0x97, 0x3C, 0x94, 0xC3, 0x97, 0xBD, 0x51, 0xE4, 0x08, 0x10,
    0xB5, 0xDB, 0x00, 0x00, 0x80, 0x93, 0x02, 0x00, 0x60, 0x95, 0x02, 0x00,
    0xE0, 0x95, 0x02, 0x00, 0xF0, 0xAF, 0x1D, 0x4B, 0x27, 0xDF, 0x37, 0x05,
    0xA0, 0x1D, 0x4B, 0x27, 0xDF, 0x37, 0x05, 0xA0, 0xAB, 0x0C, 0x9E, 0xDA,
    0xE8, 0x05, 0xA0, 0x8F, 0x09, 0x9E, 0xDA, 0xE8, 0x05, 0xA0, 0x1D, 0x8B,
    0x72, 0xE0, 0x37, 0x15, 0xA0, 0x1D, 0x0B, 0x09, 0x63, 0x9A, 0x24, 0xA0,
    0xAB, 0xEC, 0x40, 0x4F, 0xDF, 0x24, 0xA0, 0xAB, 0x6C, 0xEF, 0x14, 0x64,
    0x35, 0xA0, 0x8F, 0xE9, 0x40, 0x4F, 0xDF, 0x34, 0xA0, 0x8F, 0x69, 0xEF,
    0x14, 0x64, 0x15, 0xA0, 0x1D, 0xCB, 0xBD, 0xE1, 0xCE, 0x64, 0x50, 0xC5,
    0xAC, 0x45, 0x44, 0xD9, 0x52, 0x60, 0x3E, 0x4B, 0x36, 0xA8, 0xBD, 0x65,
    0x80, 0xB8, 0x4C, 0xC3, 0x89, 0xDC, 0x53, 0xA0, 0x1D, 0x2B, 0x62, 0x2A,
    0x1A, 0x83, 0x00, 0x76, 0xA9, 0x45, 0x44, 0xD9, 0x52, 0xF0, 0xFC, 0x4A,
    0x36, 0xA8, 0xBD, 0x85, 0xD0, 0x82, 0x49, 0xC3, 0x89, 0xDC, 0x53, 0xA0,
    0x1D, 0x4B, 0xDF, 0x64, 0x3A, 0xE4, 0xA0, 0x1D, 0x2B, 0x63, 0x6F, 0xEC,
    0xC4, 0x20, 0x46, 0xED, 0x36, 0xA8, 0x83, 0xB3, 0x00, 0xE8, 0x0C, 0x00,
    0xC0, 0x8D, 0x0D, 0x21, 0xF5, 0xE8, 0x36, 0xA8, 0x83, 0xF3, 0x40, 0x53,
    0x09, 0x00, 0xC0, 0x8D, 0xED, 0xA0, 0x1D, 0x2B, 0x0D, 0x6C, 0xB5, 0x43,
    0xB1, 0x33, 0xB0, 0x0B, 0xA8, 0x8F, 0x43, 0xF1, 0xBC, 0x4E, 0x21, 0xA8,
    0x89, 0x43, 0x21, 0x46, 0xED, 0x36, 0xA8, 0x83, 0x63, 0x91, 0x07, 0xA6,
    0x0B, 0xA8, 0x8F, 0x63, 0x61, 0x7E, 0x47, 0x21, 0xA8, 0x89, 0x63, 0x21,
    0xF5, 0xE8, 0x36, 0xA8, 0x83, 0x93, 0x01, 0xFA, 0x71, 0xF1, 0xA7, 0xAF,
    0x93, 0x71, 0x88, 0xF1, 0xF7, 0xA7, 0xA7, 0x93, 0x31, 0xDD, 0x52, 0xE4,
    0xA7, 0xBF, 0xC3, 0x41, 0x41, 0x64, 0xF1, 0xA7, 0xAF, 0xC3, 0xD1, 0xB2,
    0xE4, 0xF7, 0xA7, 0xA7, 0xC3, 0x21, 0x5E, 0x43, 0xE4, 0xA7, 0xBF, 0x13,
    0x32, 0xBC, 0x34, 0xAE, 0xA6, 0x21, 0x17, 0x22, 0xC4, 0x54, 0xBA, 0x66,
    0xA3, 0x14, 0x62, 0xA6, 0x74, 0x98, 0xA6, 0x43, 0x12, 0x32, 0x6C, 0x14,
    0x69, 0x66, 0x07, 0x10, 0x32, 0x24, 0xB4, 0x11, 0xA0, 0xBF, 0x13, 0xC2,
    0xC7, 0xB4, 0x32, 0xA3, 0xBF, 0x13, 0x32, 0xDD, 0x52, 0xE4, 0xA7, 0x89,
    0x44, 0x12, 0x7F, 0x21, 0xAE, 0xA6, 0x21, 0x47, 0x22, 0x77, 0x41, 0xBA,
    0x66, 0xA3, 0x44, 0xE2, 0x94, 0x61, 0x98, 0xA6, 0x43, 0x42, 0x12, 0xCF,
    0x01, 0x69, 0x66, 0x07, 0x40, 0x12, 0x17, 0xA2, 0x11, 0xA0, 0xBF, 0x43,
    0x82, 0x73, 0xA1, 0x32, 0xA3, 0xBF, 0x43, 0x22, 0x5E, 0x43, 0xE4, 0xA7,
    0x89, 0x54, 0x82, 0x8B, 0x35, 0xAE, 0x66, 0x29, 0x67, 0xF2, 0x9F, 0x55,
    0xBA, 0x26, 0xA3, 0x74, 0x22, 0x79, 0x75, 0x98, 0x66, 0x3C, 0x82, 0x42,
    0x1E, 0x15, 0x69, 0x26, 0x00, 0xB0, 0x42, 0xFE, 0x53, 0x51, 0xE6, 0x7B,
    0xC8, 0xD2, 0xAF, 0x20, 0xAE, 0x66, 0x29, 0xD7, 0x62, 0x9B, 0x40, 0xBA,
    0x26, 0xA3, 0xE4, 0x32, 0xC2, 0x60, 0x98, 0x66, 0x3C, 0xF2, 0x12, 0x1D,
    0x01, 0x69, 0x26, 0x00, 0x20, 0xF3, 0x3C, 0x42, 0x51, 0xE6, 0x7B, 0x38,
    0xE3, 0x20, 0x36, 0xAE, 0xA6, 0x29, 0x47, 0x53, 0x3B, 0x56, 0xBA, 0x26,
    0xA3, 0x54, 0xB3, 0x11, 0x76, 0x98, 0xA6, 0x3C, 0x62, 0x03, 0xA1, 0x15,
    0x69, 0x26, 0x00, 0x70, 0x23, 0x8C, 0x14, 0x95, 0x25, 0x0A, 0x8A, 0x63,
    0x1A, 0x20, 0xAE, 0xA6, 0x29, 0x97, 0x03, 0x00, 0x40, 0xBA, 0x26, 0xA3,
    0xA4, 0x93, 0x29, 0x60, 0x98, 0xA6, 0x3C, 0xB2, 0x53, 0x9A, 0x00, 0x69,
    0x26, 0x00, 0xC0, 0x23, 0xAF, 0x01, 0x95, 0x25, 0x0A, 0x0A,
};

static uint8_t skeleton_kinect[] = {
    0x47, 0x00, 0x00, 0x00, 0x02, 0x44, 0xED, 0x36, 0x5E, 0x39, 0x2E, 0xBF,
    0x6B, 0xB0, 0x80, 0x3C, 0xF2, 0x7B, 0x88, 0xBD, 0x51, 0xE4, 0x08, 0x10,
    0xB5, 0xDB, 0x00, 0x00, 0x00, 0x93, 0x02, 0x00, 0xD0, 0x96, 0x02, 0x00,
    0xE0, 0x94, 0x02, 0x00, 0xF0, 0xBF, 0xBF, 0x6B, 0x30, 0xA0, 0x9F, 0x04,
    0xB0, 0xBF, 0x8B, 0x34, 0xA1, 0x9F, 0x04, 0xB0, 0x4D, 0x4D, 0xA7, 0x5B,
    0x50, 0x05, 0xB0, 0x31, 0x4A, 0xA7, 0x5B, 0x50, 0x05, 0xB0, 0xBF, 0xAB,
    0x7B, 0xA1, 0x9F, 0x14, 0xB0, 0xBF, 0x8B, 0x1A, 0x26, 0x02, 0x24, 0xB0,
    0x4D, 0xCD, 0x6F, 0x4F, 0x33, 0x24, 0xB0, 0x4D, 0x8D, 0x8B, 0xD5, 0xCB,
    0x34, 0xB0, 0x31, 0xCA, 0x6F, 0x4F, 0x33, 0x34, 0xB0, 0x31, 0x8A, 0x8B,
    0xD5, 0xCB, 0x14, 0xB0, 0xBF, 0x2B, 0xCB, 0xA3, 0x36, 0x64, 0x40, 0x69,
    0xAD, 0xA1, 0x43, 0x06, 0x52, 0x60, 0xE0, 0xAB, 0x47, 0x6B, 0x25, 0x65,
    0xD0, 0x59, 0x0D, 0xA3, 0x49, 0x3D, 0x53, 0xB0, 0xBF, 0xAB, 0x73, 0xED,
    0x81, 0x82, 0x20, 0x16, 0xAA, 0xA1, 0x43, 0x06, 0x52, 0x00, 0x9F, 0xAB,
    0x47, 0x6B, 0x25, 0x85, 0x80, 0x25, 0x0A, 0xA3, 0x49, 0x3D, 0x53, 0xB0,
    0xBF, 0xCB, 0xF0, 0x27, 0xA2, 0xE3, 0xB0, 0xBF, 0x8B, 0xB4, 0x31, 0x0E,
    0xC4, 0x30, 0xE8, 0x6D, 0x48, 0x6B, 0xEB, 0xB2, 0xC0, 0x86, 0x0D, 0x00,
    0xC0, 0x1F, 0x0B, 0x31, 0x97, 0x69, 0x48, 0x6B, 0xEB, 0xF2, 0x90, 0xF8,
    0x09, 0x00, 0xC0, 0x1F, 0xEB, 0xB0, 0xBF, 0xAB, 0xDE, 0xEE, 0x05, 0x43,
    0x01, 0xB5, 0x31, 0x10, 0xEB, 0xFA, 0x42, 0x91, 0xCE, 0xCF, 0x33, 0x2B,
    0xF1, 0x42, 0xA1, 0xFB, 0x6D, 0x48, 0x6B, 0xEB, 0x62, 0x61, 0xCA, 0x25,
    0x10, 0xEB, 0xFA, 0x62, 0xD1, 0xB0, 0xC7, 0x33, 0x2B, 0xF1, 0x62, 0xC1,
    0x83, 0x69, 0x48, 0x6B, 0xEB, 0x92, 0xB1, 0xA8, 0x73, 0xF3, 0x2A, 0x1E,
    0x93, 0xC1, 0x2B, 0x93, 0xFD, 0xEA, 0x11, 0x93, 0x91, 0xA2, 0xF4, 0xE4,
    0xAA, 0x2F, 0xC3, 0xA1, 0xD6, 0x63, 0xF3, 0x2A, 0x1E, 0xC3, 0x91, 0x53,
    0x84, 0xFD, 0xEA, 0x11, 0xC3, 0xC1, 0xDC, 0xE2, 0xE4, 0xAA, 0x2F, 0x13,
    0xB2, 0x39, 0x76, 0xDD, 0xA9, 0x0F, 0x16, 0x82, 0x40, 0xB6, 0xE7, 0x29,
    0xF1, 0x13, 0x32, 0x27, 0xF6, 0xCA, 0xA9, 0xEC, 0x11, 0xC2, 0xF5, 0x95,
    0xA2, 0x69, 0x06, 0x10, 0x92, 0xB8, 0xB5, 0x3E, 0xA4, 0x2F, 0x13, 0x92,
    0x43, 0x96, 0xE7, 0xA6, 0x2F, 0x13, 0x92, 0xA2, 0xF4, 0xE4, 0x6A, 0xDB,
    0x43, 0xA2, 0x45, 0x61, 0xDD, 0xA9, 0x0F, 0x46, 0xD2, 0x3E, 0xA1, 0xE7,
    0x29, 0xF1, 0x43, 0x22, 0x58, 0xE1, 0xCA, 0xA9, 0xEC, 0x41, 0xA2, 0x89,
    0x81, 0xA2, 0x69, 0x06, 0x40, 0xC2, 0xC6, 0xA1, 0x3E, 0xA4, 0x2F, 0x43,
    0xD2, 0x3B, 0x81, 0xE7, 0xA6, 0x2F, 0x43, 0xD2, 0xDC, 0xE2, 0xE4, 0x6A,
    0xDB, 0x53, 0xF2, 0xE9, 0x76, 0xDD, 0x69, 0x16, 0x66, 0x52, 0xFB, 0xB6,
    0xE7, 0xE9, 0xF0, 0x73, 0x52, 0xDA, 0xF6, 0xCA, 0xA9, 0xE6, 0x81, 0x12,
    0x8D, 0x96, 0xA2, 0x29, 0x00, 0xB0, 0x52, 0x98, 0x75, 0x8E, 0x29, 0x36,
    0xC7, 0x72, 0x95, 0x60, 0xDD, 0x69, 0x16, 0xD6, 0x12, 0x84, 0xA0, 0xE7,
    0xE9, 0xF0, 0xE3, 0x12, 0xA5, 0xE0, 0xCA, 0xA9, 0xE6, 0xF1, 0x52, 0xF2,
    0x80, 0xA2, 0x29, 0x00, 0x20, 0x03, 0xE7, 0x61, 0x8E, 0x29, 0x36, 0x37,
    0xF3, 0x68, 0x77, 0xDD, 0x69, 0x16, 0x46, 0x63, 0x7F, 0xB7, 0xE7, 0x29,
    0xF1, 0x53, 0x03, 0x5C, 0xF7, 0xCA, 0xA9, 0xE6, 0x61, 0x43, 0xFC, 0x96,
    0xA2, 0x29, 0x00, 0x70, 0xE3, 0x10, 0x76, 0xEE, 0xA8, 0x88, 0x88, 0x73,
    0x16, 0x60, 0xDD, 0x69, 0x16, 0x96, 0x03, 0x00, 0xA0, 0xE7, 0x29, 0xF1,
    0xA3, 0x53, 0x23, 0xE0, 0xCA, 0xA9, 0xE6, 0xB1, 0x23, 0x83, 0x80, 0xA2,
    0x29, 0x00, 0xC0, 0x73, 0x6E, 0x61, 0xEE, 0xA8, 0x88, 0x08,
};

static BodyType GetBodyType(const X_AVATAR_METADATA& metadata) {
  const AssetId male_body_asset_id = {
      2, 0, 1, {0xC1, 0xC8, 0xF1, 0x09, 0xA1, 0x9C, 0xB2, 0xE0}};
  const AssetId female_body_asset_id = {
      2, 0, 2, {0xC1, 0xC8, 0xF1, 0x09, 0xA1, 0x9C, 0xB2, 0xE0}};
  if (metadata.body_component.asset_id == male_body_asset_id) {
    return BodyType::kMale;
  }
  if (metadata.body_component.asset_id == female_body_asset_id) {
    return BodyType::kFemale;
  }
  return BodyType::kUnknown;
}

static void SaveModel(const X_AVATAR_COMPONENT_INFO& component_info,
                      std::shared_ptr<Model> model, AssetPack* asset_pack) {
  if (model == nullptr) {
    return;
  }

  auto asset_id = component_info.asset_id.to_string();
  auto asset_name =
      xe::to_utf8(asset_pack->GetAssetName(component_info.asset_id));

  size_t batch_index = 0;
  for (const auto& triangle_batch : model->triangle_batches) {
    std::string output_name;
    if (!asset_name.size()) {
      output_name = fmt::format("avatars\\{}_{}.obj", asset_id, batch_index++);
    } else {
      output_name = fmt::format("avatars\\{}_{}_{}.obj", asset_id, asset_name,
                                batch_index++);
    }
    FILE* output = fopen(output_name.c_str(), "wb");

    for (const auto& vertex : triangle_batch.vertices) {
      auto vertex_line = fmt::format("v {} {} {}\r\n", vertex.position.x,
                                     vertex.position.y, vertex.position.z);
      fwrite(vertex_line.c_str(), 1, vertex_line.size(), output);
    }

    for (size_t i = 0; i < triangle_batch.indices.size(); i += 3) {
      auto index0 = 1 + triangle_batch.indices[i + 0];
      auto index1 = 1 + triangle_batch.indices[i + 1];
      auto index2 = 1 + triangle_batch.indices[i + 2];
      auto line = fmt::format("f {} {} {}\r\n", index0, index1, index2);
      fwrite(line.c_str(), 1, line.size(), output);
    }

    fflush(output);
    fclose(output);
  }

  size_t texture_index = 0;
  for (const auto& model_texture : model->textures) {
    const auto& texture = model_texture.texture;
    struct {
      uint32_t size;
      uint32_t flags;
      uint32_t height;
      uint32_t width;
      uint32_t pitch_or_linear_size;
      uint32_t depth;
      uint32_t mip_levels;
      uint32_t reserved1[11];
      struct {
        uint32_t size;
        uint32_t flags;
        be<fourcc_t> fourcc;
        uint32_t rgb_bit_count;
        uint32_t r_bit_mask;
        uint32_t g_bit_mask;
        uint32_t b_bit_mask;
        uint32_t a_bit_mask;
      } pixel_format;
      uint32_t caps[4];
      uint32_t reserved2;
    } dds_header;

    auto format = texture.format & 0x3F;

    std::memset(&dds_header, 0, sizeof(dds_header));
    dds_header.size = sizeof(dds_header);
    dds_header.flags = 1u | 2u | 4u | 0x1000u | 0x20000u;
    if ((format >= 18 && format <= 20) || format == 49 ||
        (format >= 58 && format <= 60)) {
      dds_header.flags |= 0x80000u;
    } else {
      dds_header.flags |= 0x8u;
    }
    dds_header.height = texture.height;
    dds_header.width = texture.width;
    dds_header.pitch_or_linear_size = texture.data_stride;
    dds_header.mip_levels = 1;

    dds_header.pixel_format.size = sizeof(dds_header.pixel_format);
    switch (format) {
      case 18: {
        dds_header.pixel_format.flags = 0x4u;
        dds_header.pixel_format.fourcc = make_fourcc("DXT1");
        break;
      }
      case 19: {
        dds_header.pixel_format.flags = 0x4u;
        dds_header.pixel_format.fourcc = make_fourcc("DXT3");
        break;
      }
      case 20: {
        dds_header.pixel_format.flags = 0x4u;
        dds_header.pixel_format.fourcc = make_fourcc("DXT5");
        break;
      }
      case 6: {
        dds_header.pixel_format.flags = 0x1u | 0x40u;
        dds_header.pixel_format.rgb_bit_count = 32;
        dds_header.pixel_format.r_bit_mask = 0x00FF0000u;
        dds_header.pixel_format.g_bit_mask = 0x0000FF00u;
        dds_header.pixel_format.b_bit_mask = 0x000000FFu;
        dds_header.pixel_format.a_bit_mask = 0xFF000000u;
        break;
      }
      default: {
        assert_unhandled_case(src.format);
        std::memset(&dds_header.pixel_format, 0xCD,
                    sizeof(dds_header.pixel_format));
        XELOGW("Skipping {} for texture dump.", format);
        texture_index++;
        continue;
      }
    }

    std::string output_name;
    if (!asset_name.size()) {
      output_name = fmt::format("avatars\\{}_{}_{:X}.dds", asset_id,
                                texture_index++, texture.format);
    } else {
      output_name = fmt::format("avatars\\{}_{}_{}_{:X}.dds", asset_id,
                                asset_name, texture_index++, texture.format);
    }
    FILE* output = fopen(output_name.c_str(), "wb");
    const char signature[4] = {'D', 'D', 'S', ' '};
    fwrite(&signature, sizeof(signature), 1, output);
    fwrite(&dds_header, sizeof(dds_header), 1, output);

    std::vector<uint8_t> data_bytes(texture.data_bytes);
    for (size_t i = 0; i < data_bytes.size(); i += 2) {
      auto b = data_bytes[i];
      data_bytes[i] = data_bytes[i + 1];
      data_bytes[i + 1] = b;
    }

    fwrite(data_bytes.data(), 1, data_bytes.size(), output);
    fclose(output);
  }
}

static bool LoadFile(std::filesystem::path path, std::vector<uint8_t>& buffer) {
  bool was_loaded = false;
  auto handle = xe::filesystem::OpenFile(path, "rb");
  if (handle != nullptr) {
    fseek(handle, 0, SEEK_END);
    buffer.resize(ftell(handle));
    fseek(handle, 0, SEEK_SET);
    size_t bytes_read = fread(buffer.data(), 1, buffer.size(), handle);
    if (bytes_read == buffer.size()) {
      was_loaded = true;
    }
    fclose(handle);
  }
  return was_loaded;
}

struct ShaderParameterOverride {
  uint32_t usage;
  float x;
  float y;
  float z;
  float w;
};

static bool VertexToGuest(X_AVATAR_VERTEX* guest, const Vertex& host) {
  guest->position.x = host.position.x;
  guest->position.y = host.position.y;
  guest->position.z = host.position.z;
  guest->normal = host.normal;
  guest->blend_weight = host.blend_weight;
  guest->blend_indices = host.blend_indices;
  guest->color = host.color;
  for (size_t i = 0; i < host.uvs.size(); ++i) {
    auto& guest_uv = guest->uvs[i];
    const auto& host_uv = host.uvs[i];
    guest_uv.u = host_uv.x;
    guest_uv.v = host_uv.y;
  }
  return true;
}

static void OverrideShaderParameter(
    X_AVATAR_SHADER_PARAM* parameters,
    const ShaderParameterOverride& parameter_override) {
  for (size_t i = 0, o = 19; i < 20; ++i, --o) {
    auto& parameter = parameters[o];
    if (parameter.usage != parameter_override.usage) {
      continue;
    }
    assert_true(
        parameter.type == uint32_t(ShaderParameterType::kPixelConstant) ||
        parameter.type == uint32_t(ShaderParameterType::kVertexConstant));
    parameter.constant_values[0] = parameter_override.x;
    parameter.constant_values[1] = parameter_override.y;
    parameter.constant_values[2] = parameter_override.z;
    parameter.constant_values[3] = parameter_override.w;
  }
}

static bool TriangleBatchToGuest(
    X_AVATAR_TRIANGLE_BATCH& guest, const TriangleBatch& host,
    MemoryBlock* cpu_memory, uint8_t* cpu_buffer, MemoryBlock* gpu_memory,
    uint8_t* gpu_buffer, uint32_t gpu_buffer_base_ptr,
    const std::vector<ShaderParameterOverride>& shader_parameter_overrides) {
  guest.shader_id = host.shader_id;
  std::memset(guest.shader_parameters, 0, sizeof(guest.shader_parameters));
  for (size_t i = 0; i < host.shader_parameters.size(); ++i) {
    auto& guest_parameter = guest.shader_parameters[i];
    const auto& host_parameter = host.shader_parameters[i];
    guest_parameter.type = static_cast<uint32_t>(host_parameter.type);
    guest_parameter.usage = host_parameter.usage;
    if (host_parameter.type == ShaderParameterType::kTexture) {
      guest_parameter.texture.index = host_parameter.texture.index;
      guest_parameter.texture.uv_index = host_parameter.texture.uv_layer;
      guest_parameter.texture.flags = host_parameter.texture.flags;
    } else {
      for (size_t j = 0; j < 4; ++j) {
        guest_parameter.constant_values[j] = host_parameter.constant_values[j];
      }
    }
  }

  for (const auto& shader_parameter_override : shader_parameter_overrides) {
    OverrideShaderParameter(guest.shader_parameters, shader_parameter_override);
  }

  guest.triangle_count = host.triangle_count;
  guest.vertex_count = static_cast<uint32_t>(host.vertices.size());
  guest.uv_count = host.uv_count;
  guest.vertex_size = host.vertex_size;
  guest.index_size = host.index_size;
  gpu_memory->SetPointer(&guest.vertices_ptr,
                         gpu_buffer_base_ptr + host.vertex_array_offset);
  gpu_memory->SetPointer(&guest.indices_ptr,
                         gpu_buffer_base_ptr + host.index_array_offset);
  uint32_t vertex_offset = host.vertex_array_offset;
  for (size_t i = 0; i < host.vertices.size(); ++i) {
    if (!VertexToGuest(
            reinterpret_cast<X_AVATAR_VERTEX*>(&gpu_buffer[vertex_offset]),
            host.vertices[i])) {
      return false;
    }
    vertex_offset += guest.vertex_size;
  }
  uint32_t index_offset = host.index_array_offset;
  for (size_t i = 0; i < host.indices.size(); ++i) {
    auto guest_index =
        reinterpret_cast<be<uint16_t>*>(&gpu_buffer[index_offset]);
    *guest_index = host.indices[i];
    index_offset += guest.index_size;
  }
  return true;
}

static bool TextureToGuest(X_AVATAR_TEXTURE& guest, const ModelTexture& host,
                           MemoryBlock* cpu_memory, uint8_t* cpu_buffer,
                           MemoryBlock* gpu_memory, uint8_t* gpu_buffer,
                           uint32_t gpu_buffer_base_ptr) {
  auto format = host.texture.format;
  format &= ~0x00000100u;
  guest.format = format;
  guest.width = host.texture.width;
  guest.height = host.texture.height;
  guest.total_base_size = host.texture.total_data_size;
  guest.total_mip_size = 0;
  guest.base_size = host.texture.data_size;
  guest.mip_size = 0;
  guest.mip_levels = 1;
  guest.layer_count = host.texture.layer_count;
  gpu_memory->SetPointer(&guest.base_data_ptr,
                         gpu_buffer_base_ptr + host.gpu_offset);
  guest.mip_data_ptr = 0;

  auto data_buffer = &gpu_buffer[host.gpu_offset];
  if (!host.texture.is_empty) {
    uint32_t output_align = (host.texture.format == 0x1A200152u ||
                             host.texture.format == 0x1A200052u)
                                ? 256u
                                : 512u;
    size_t input_stride = host.texture.data_stride;
    size_t output_stride = align(host.texture.data_stride, output_align);
    size_t input_offset = 0;
    size_t output_offset = 0;
    assert_true(host.texture.data_rows * output_stride <= host.gpu_size);
    for (size_t layer = 0; layer < host.texture.layer_count; ++layer) {
      for (size_t y = 0; y < host.texture.data_rows; ++y) {
        std::memcpy(&data_buffer[output_offset],
                    &host.texture.data_bytes.data()[input_offset],
                    input_stride);
        input_offset += input_stride;
        output_offset += output_stride;
      }
    }
  }
  return true;
}

static bool ModelToGuest(
    std::shared_ptr<Model> host_model, X_AVATAR_MODEL* guest_model,
    MemoryBlock* cpu_memory, MemoryBlock* gpu_memory, uint32_t category_mask,
    std::shared_ptr<Texture> replacement_textures[6],
    const std::vector<ShaderParameterOverride>& shader_parameter_overrides) {
  if (host_model == nullptr) {
    return false;
  }

  uint32_t cpu_buffer_ptr, gpu_buffer_ptr;
  uint8_t* cpu_buffer =
      cpu_memory->ClaimBytes(&cpu_buffer_ptr, host_model->cpu_size);
  uint8_t* gpu_buffer =
      gpu_memory->ClaimBytes(&gpu_buffer_ptr, host_model->gpu_size);

  *guest_model = {};
  guest_model->cpu_size = host_model->cpu_size;
  guest_model->gpu_size = host_model->gpu_size;
  guest_model->texture_size = host_model->texture_buffer_size;
  guest_model->vertex_size = host_model->vertex_buffer_size;
  guest_model->index_size = host_model->index_buffer_size;
  guest_model->triangle_batch_count =
      static_cast<uint32_t>(host_model->triangle_batches.size());
  guest_model->texture_count =
      static_cast<uint32_t>(host_model->textures.size());
  cpu_memory->SetPointer(&guest_model->cpu_buffer_ptr, cpu_buffer_ptr);
  gpu_memory->SetPointer(&guest_model->gpu_buffer_ptr, gpu_buffer_ptr);
  gpu_memory->SetPointer(&guest_model->vertex_buffer_ptr,
                         gpu_buffer_ptr + host_model->vertex_buffer_offset);
  gpu_memory->SetPointer(&guest_model->index_buffer_ptr,
                         gpu_buffer_ptr + host_model->index_buffer_offset);
  cpu_memory->SetPointer(
      &guest_model->triangle_batches_ptr,
      cpu_buffer_ptr + host_model->triangle_batch_array_offset);
  cpu_memory->SetPointer(&guest_model->textures_ptr,
                         cpu_buffer_ptr + host_model->texture_array_offset);

  auto guest_triangle_batches = reinterpret_cast<X_AVATAR_TRIANGLE_BATCH*>(
      &cpu_buffer[host_model->triangle_batch_array_offset]);
  for (size_t i = 0; i < guest_model->triangle_batch_count; ++i) {
    if (!TriangleBatchToGuest(guest_triangle_batches[i],
                              host_model->triangle_batches[i], cpu_memory,
                              cpu_buffer, gpu_memory, gpu_buffer,
                              gpu_buffer_ptr, shader_parameter_overrides)) {
      return false;
    }
  }

  // override head textures with those from the metadata
  if (category_mask == ComponentCategory::kHead) {
    int usage_indices[20];
    for (size_t i = 0; i < 20; ++i) {
      usage_indices[i] = -1;
    }
    const int usage_to_replacement_texture_indices[] = {
        -1, -1, -1, -1, -1, 5, 3, 2, 2, 1, 1, 4, 0,
    };
    std::vector<int> replacement_texture_indices(guest_model->texture_count);
    for (size_t i = 0; i < guest_model->triangle_batch_count; ++i) {
      const auto& triangle_batch = host_model->triangle_batches[i];
      for (const auto& shader_parameter : triangle_batch.shader_parameters) {
        if (shader_parameter.usage >= 20 ||
            shader_parameter.type != ShaderParameterType::kTexture) {
          continue;
        }
        const auto& usage = shader_parameter.usage;
        const auto& texture_index = shader_parameter.texture.index;
        assert_true(usage_indices[usage] == -1 ||
                    usage_indices[usage] == texture_index);
        usage_indices[usage] = texture_index;
        replacement_texture_indices[texture_index] =
            usage < countof(usage_to_replacement_texture_indices)
                ? usage_to_replacement_texture_indices[usage]
                : -1;
      }
    }

    auto guest_textures = reinterpret_cast<X_AVATAR_TEXTURE*>(
        &cpu_buffer[host_model->texture_array_offset]);
    for (size_t i = 0; i < guest_model->texture_count; ++i) {
      ModelTexture model_texture;
      auto replacement_texture_index = replacement_texture_indices[i];
      if (replacement_texture_index >= 0 &&
          replacement_textures[replacement_texture_index] != nullptr) {
        model_texture.gpu_offset = host_model->textures[i].gpu_offset;
        model_texture.gpu_size = host_model->textures[i].gpu_size;
        model_texture.texture =
            *replacement_textures[replacement_texture_index];
      } else {
        model_texture = host_model->textures[i];
      }
      if (!TextureToGuest(guest_textures[i], model_texture, cpu_memory,
                          cpu_buffer, gpu_memory, gpu_buffer, gpu_buffer_ptr)) {
        return false;
      }
    }
  } else {
    auto guest_textures = reinterpret_cast<X_AVATAR_TEXTURE*>(
        &cpu_buffer[host_model->texture_array_offset]);
    for (size_t i = 0; i < guest_model->texture_count; ++i) {
      if (!TextureToGuest(guest_textures[i], host_model->textures[i],
                          cpu_memory, cpu_buffer, gpu_memory, gpu_buffer,
                          gpu_buffer_ptr)) {
        return false;
      }
    }
  }

  return true;
}

bool SkeletonToGuest(X_AVATAR_SKELETON* guest, std::shared_ptr<Skeleton> host,
                     MemoryBlock* cpu_memory) {
  assert_true(host->joints.size() <= 72);
  uint8_t joint_count = static_cast<uint8_t>(host->joints.size());

  *guest = {};
  guest->joint_count = joint_count;

  const uint8_t invalid_index = 255;

  uint32_t guest_joints_ptr;
  auto guest_joints = cpu_memory->Claim<X_AVATAR_SKELETON_JOINT>(
      &guest_joints_ptr, joint_count);
  cpu_memory->SetPointer(&guest->joints_ptr, guest_joints_ptr);

  for (uint8_t i = 0; i < joint_count; ++i) {
    auto& guest_joint = guest_joints[i];
    const auto& host_joint = host->joints[i];
    guest_joint.parent_index = host_joint.parent_index;
    guest_joint.first_child_index = host_joint.first_child_index;
    guest_joint.next_index = host_joint.next_index;
    guest_joint.bindpose.position.x = host_joint.bindpose.position.x;
    guest_joint.bindpose.position.y = host_joint.bindpose.position.y;
    guest_joint.bindpose.position.z = host_joint.bindpose.position.z;
    guest_joint.bindpose.position.w = 1.f;
    guest_joint.bindpose.rotation.x = host_joint.bindpose.rotation.x;
    guest_joint.bindpose.rotation.y = host_joint.bindpose.rotation.y;
    guest_joint.bindpose.rotation.z = host_joint.bindpose.rotation.z;
    guest_joint.bindpose.rotation.w = host_joint.bindpose.rotation.w;
    guest_joint.pose.position.x = host_joint.pose.position.x;
    guest_joint.pose.position.y = host_joint.pose.position.y;
    guest_joint.pose.position.z = host_joint.pose.position.z;
    guest_joint.pose.position.w = 1.f;
    guest_joint.pose.rotation.x = host_joint.pose.rotation.x;
    guest_joint.pose.rotation.y = host_joint.pose.rotation.y;
    guest_joint.pose.rotation.z = host_joint.pose.rotation.z;
    guest_joint.pose.rotation.w = host_joint.pose.rotation.w;
    guest_joint.pose.scale.x = host_joint.pose.scale.x;
    guest_joint.pose.scale.y = host_joint.pose.scale.y;
    guest_joint.pose.scale.z = host_joint.pose.scale.z;
    guest_joint.pose.scale.w = 1.f;
  }

  return true;
}

static std::shared_ptr<Model> LoadModelAsset(
    AssetPack* asset_pack, const X_AVATAR_COMPONENT_INFO& info,
    ModelLoadOptions model_load_options) {
  const uint8_t* strb_buffer;
  size_t strb_size;
  std::vector<uint8_t> strb_bytes;
  if (!asset_pack->GetAssetData(info.asset_id, strb_buffer, strb_size)) {
    // TODO(gibbed): load from user data path
    std::filesystem::path bin_path =
        fmt::format("avatar_blobs\\{}.bin", info.asset_id.to_string());
    if (!LoadFile(bin_path, strb_bytes)) {
      return nullptr;
    }
    strb_buffer = strb_bytes.data();
    strb_size = strb_bytes.size();
  }
  return Model::Load(strb_buffer, strb_size, model_load_options);
}

static std::shared_ptr<Texture> LoadTextureAsset(
    AssetPack* asset_pack, const X_AVATAR_METADATA_TEXTURE& info) {
  const uint8_t* strb_buffer;
  size_t strb_size;
  std::vector<uint8_t> strb_bytes;
  if (!asset_pack->GetAssetData(info.asset_id, strb_buffer, strb_size)) {
    // TODO(gibbed): load from user data path
    std::filesystem::path bin_path =
        fmt::format("avatar_blobs\\{}.bin", info.asset_id.to_string());
    if (!LoadFile(bin_path, strb_bytes)) {
      return nullptr;
    }
    strb_buffer = strb_bytes.data();
    strb_size = strb_bytes.size();
  }
  return Texture::Load(strb_buffer, strb_size);
}

static void GetShaderOverrides(
    const X_AVATAR_METADATA& metadata, uint32_t category_mask,
    std::vector<ShaderParameterOverride>& shader_parameter_overrides) {
  // skin color
  if (category_mask & ComponentCategory::kBody) {
    auto color = metadata.colors[0];
    ShaderParameterOverride shader_parameter_override;
    shader_parameter_override.usage = 22;
    shader_parameter_override.x = ((color >> 16) & 0xFF) / 255.f;
    shader_parameter_override.y = ((color >> 8) & 0xFF) / 255.f;
    shader_parameter_override.z = ((color >> 0) & 0xFF) / 255.f;
    shader_parameter_override.w = ((color >> 24) & 0xFF) / 255.f;
    shader_parameter_overrides.push_back(shader_parameter_override);
  }
  // hair color
  else if (category_mask & ComponentCategory::kHair) {
    auto color = metadata.colors[1];
    ShaderParameterOverride shader_parameter_override;
    shader_parameter_override.usage = 22;
    shader_parameter_override.x = ((color >> 16) & 0xFF) / 255.f;
    shader_parameter_override.y = ((color >> 8) & 0xFF) / 255.f;
    shader_parameter_override.z = ((color >> 0) & 0xFF) / 255.f;
    shader_parameter_override.w = ((color >> 24) & 0xFF) / 255.f;
    shader_parameter_overrides.push_back(shader_parameter_override);
  } else {
    ShaderParameterOverride shader_parameter_override;
    shader_parameter_override.usage = 22;
    shader_parameter_override.x = 1.f;
    shader_parameter_override.y = 1.f;
    shader_parameter_override.z = 1.f;
    shader_parameter_override.w = 1.f;
    shader_parameter_overrides.push_back(shader_parameter_override);
  }

  if (category_mask & ComponentCategory::kHead) {
    for (uint32_t i = 0, usage = 13; i < 9; ++i, ++usage) {
      auto color = metadata.colors[i];
      ShaderParameterOverride shader_parameter_override;
      shader_parameter_override.usage = usage;
      shader_parameter_override.x = ((color >> 16) & 0xFF) / 255.f;
      shader_parameter_override.y = ((color >> 8) & 0xFF) / 255.f;
      shader_parameter_override.z = ((color >> 0) & 0xFF) / 255.f;
      shader_parameter_override.w = ((color >> 24) & 0xFF) / 255.f;
      shader_parameter_overrides.push_back(shader_parameter_override);
    }
  }

  // rim light color
  {
    auto color = metadata.colors[0];
    ShaderParameterOverride shader_parameter_override;
    shader_parameter_override.usage = 27;
    // TODO(gibbed): calculate rim light color properly
    shader_parameter_override.x = 0.54901963f;
    shader_parameter_override.y = 0.5019608f;
    shader_parameter_override.z = 0.40392157f;
    shader_parameter_override.w = 1.6f;
    shader_parameter_overrides.push_back(shader_parameter_override);
  }
}

bool LoadAssetsToGuest(const X_AVATAR_METADATA& metadata,
                       uint32_t category_mask, uint32_t flags,
                       AssetPack* asset_pack, MemoryBlock* cpu_memory,
                       MemoryBlock* gpu_memory, uint32_t skeleton_version,
                       uint32_t coordinate_system) {
  category_mask &= ~ComponentCategory::kProp;

  SkeletonLoadOptions skeleton_load_options = SkeletonLoadOption::kNone;
  ModelLoadOptions model_load_options = ModelLoadOption::kNone;
  if (coordinate_system == 0) {
    skeleton_load_options |= SkeletonLoadOption::kInvert;
    model_load_options |= ModelLoadOption::kInvert;
  }

  BodyType bodyType = GetBodyType(metadata);

  std::shared_ptr<Skeleton> skeleton;

  if (skeleton_version == 1) {
    skeleton = Skeleton::Read(skeleton_nxe, countof(skeleton_nxe),
                              skeleton_load_options);
    ApplyScalesToSkeletonV1(bodyType, metadata.weight_factor,
                            metadata.height_factor, skeleton);
  } else if (skeleton_version == 2) {
    skeleton = Skeleton::Read(skeleton_kinect, countof(skeleton_kinect),
                              skeleton_load_options);
    ApplyScalesToSkeletonV2(bodyType, metadata.weight_factor,
                            metadata.height_factor, skeleton);
  } else {
    XELOGW("Unknown avatar skeleton version {}!", skeleton_version);
    return false;
  }

  // TODO(gibbed): apply blend shapes to skeleton

  std::vector<X_AVATAR_COMPONENT_INFO> source_component_infos;
  if (metadata.body_component.matches(category_mask)) {
    source_component_infos.push_back(metadata.body_component);
  }
  if (metadata.head_component.matches(category_mask)) {
    source_component_infos.push_back(metadata.head_component);
  }
  for (const auto& component : metadata.components) {
    if (component.matches(category_mask)) {
      source_component_infos.push_back(component);
    }
  }

  size_t component_failures = 0;
  std::vector<std::pair<X_AVATAR_COMPONENT_INFO, std::shared_ptr<Model>>>
      source_components;
  for (const auto& source_info : source_component_infos) {
    auto model = LoadModelAsset(asset_pack, source_info, model_load_options);
    if (model != nullptr) {
      // SaveModel(source_info, model, asset_pack);
      source_components.push_back({source_info, model});
      continue;
    }
    XELOGE("Failed to load avatar asset {}, looking for fallback...",
           source_info.asset_id.to_string());
    X_AVATAR_COMPONENT_INFO fallback_info{};
    for (const auto& candidate_info : metadata.fallback_components) {
      if (candidate_info.categories == source_info.categories) {
        // TODO(gibbed): if this fails... fall back even further?
        model = LoadModelAsset(asset_pack, candidate_info, model_load_options);
        fallback_info = candidate_info;
        break;
      }
    }
    if (model == nullptr) {
      if (!fallback_info.asset_id.is_zero()) {
        XELOGE("Failed to load fallback avatar asset {}!",
               fallback_info.asset_id.to_string());
        component_failures++;
      }
      continue;
    }
    source_components.push_back({fallback_info, model});
  }

  size_t replacement_texture_failures = 0;
  std::shared_ptr<Texture> replacement_textures[6];
  for (size_t i = 0; i < 6; ++i) {
    const auto& texture_info = metadata.textures[i];
    if (texture_info.asset_id.is_zero()) {
      continue;
    }
    auto texture = LoadTextureAsset(asset_pack, texture_info);
    if (texture == nullptr) {
      XELOGE("Failed to load avatar replacement texture {}!",
             texture_info.asset_id.to_string());
      replacement_texture_failures++;
      continue;
    }
    replacement_textures[i] = texture;
  }

  auto assets = cpu_memory->Claim<X_AVATAR_ASSETS>();
  *assets = {};

  uint32_t guest_skeleton_ptr;
  auto guest_skeleton =
      cpu_memory->Claim<X_AVATAR_SKELETON>(&guest_skeleton_ptr);

  if (!SkeletonToGuest(guest_skeleton, skeleton, cpu_memory)) {
    return false;
  }

  cpu_memory->SetPointer(&assets->skeleton_ptr, guest_skeleton_ptr);

  if (source_components.size()) {
    uint32_t component_infos_ptr;
    auto component_infos = cpu_memory->Claim<X_AVATAR_COMPONENT_INFO>(
        &component_infos_ptr, source_components.size());
    uint32_t component_models_ptr;
    auto component_models = cpu_memory->Claim<X_AVATAR_MODEL>(
        &component_models_ptr, source_components.size());

    assets->component_count = uint32_t(source_components.size());

    cpu_memory->SetPointer(&assets->component_infos_ptr, component_infos_ptr);
    cpu_memory->SetPointer(&assets->component_models_ptr, component_models_ptr);

    auto component_info = component_infos;
    auto component_model = component_models;
    for (const auto& source_component : source_components) {
      *component_info = source_component.first;

      std::vector<ShaderParameterOverride> shader_parameter_overrides;
      GetShaderOverrides(metadata, source_component.first.categories,
                         shader_parameter_overrides);

      ModelToGuest(source_component.second, component_model, cpu_memory,
                   gpu_memory, source_component.first.categories,
                   replacement_textures, shader_parameter_overrides);
      component_info++;
      component_model++;
    }
  } else {
    assets->component_count = 0;
  }

  return true;
}

}  // namespace avatars
}  // namespace xe
