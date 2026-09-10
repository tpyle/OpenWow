#include "openwow/render/world/terrain/terrain_mesh.h"
#include "openwow/render/world/terrain/terrain_material_compositor.h"

#include "openwow/foundation/diagnostics/logging.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace openwow::render {

using namespace data::terrain;

static void InitializeBounds(float bounds_min[3], float bounds_max[3]) {
  std::fill_n(bounds_min, 3, std::numeric_limits<float>::max());
  std::fill_n(bounds_max, 3, std::numeric_limits<float>::lowest());
}

static void ExpandBounds(float bounds_min[3], float bounds_max[3], const float wx, const float wy,
                         const float wz) {
  bounds_min[0] = std::min(bounds_min[0], wx);
  bounds_min[1] = std::min(bounds_min[1], wy);
  bounds_min[2] = std::min(bounds_min[2], wz);
  bounds_max[0] = std::max(bounds_max[0], wx);
  bounds_max[1] = std::max(bounds_max[1], wy);
  bounds_max[2] = std::max(bounds_max[2], wz);
}

static constexpr int OuterIndex(int r, int c) {
  return r * 17 + c;
}

static constexpr int InnerIndex(int r, int c) {
  return r * 17 + 9 + c;
}

static bool IsHole(uint32_t holes, int row, int col) {
  const int hole_row = row / 2;
  const int hole_col = col / 2;
  const int bit = hole_row * 4 + hole_col;
  return (holes & (1u << bit)) != 0;
}

static void AppendChunkVertices(std::vector<TerrainVertex> &vertices, float bounds_min[3],
                                float bounds_max[3], const TerrainChunk &chunk,
                                const std::uint32_t chunk_x, const std::uint32_t chunk_y) {
  const std::size_t vertex_base = vertices.size();
  vertices.resize(vertex_base + kVerticesPerChunk);

  const float base_x = chunk.header.position_x;
  const float base_y = chunk.header.position_y;
  const float base_z = chunk.header.position_z;

  const bool has_colors = chunk.vertex_colors.size() == kVerticesPerChunk;

  auto EmitVertex = [&](int local_idx, float wx, float wy, float wz, float u, float v) {
    TerrainVertex &vert = vertices[vertex_base + static_cast<std::size_t>(local_idx)];
    vert.position[0] = wx;
    vert.position[1] = wy;
    vert.position[2] = wz;

    const auto normal =
        data::terrain::UnpackNormal(chunk.normals[static_cast<std::size_t>(local_idx)]);
    vert.normal[0] = normal.x;
    vert.normal[1] = normal.y;
    vert.normal[2] = normal.z;
    vert.texcoord[0] = u;
    vert.texcoord[1] = v;
    constexpr float atlas_scale =
        static_cast<float>(kAlphaMapSize - 1) / static_cast<float>(kTerrainAlphaAtlasSize);
    const float atlas_offset_x = (static_cast<float>(chunk_x * kAlphaMapSize) + 0.5f) /
                                 static_cast<float>(kTerrainAlphaAtlasSize);
    const float atlas_offset_y = (static_cast<float>(chunk_y * kAlphaMapSize) + 0.5f) /
                                 static_cast<float>(kTerrainAlphaAtlasSize);
    vert.alpha_texcoord[0] = u * atlas_scale + atlas_offset_x;
    vert.alpha_texcoord[1] = v * atlas_scale + atlas_offset_y;

    if (has_colors) {
      const auto &vc = chunk.vertex_colors[static_cast<std::size_t>(local_idx)];

      vert.color = static_cast<uint32_t>(vc.b)
                   | (static_cast<uint32_t>(vc.g) << 8)
                   | (static_cast<uint32_t>(vc.r) << 16)
                   | (static_cast<uint32_t>(vc.a) << 24);
    } else {
      vert.color = 0xFF7F7F7Fu;
    }
    ExpandBounds(bounds_min, bounds_max, wx, wy, wz);
  };

  for (int r = 0; r < kOuterGrid; ++r) {
    for (int c = 0; c < kOuterGrid; ++c) {
      const int local_idx = OuterIndex(r, c);
      const float wx = base_x - static_cast<float>(r) * kUnitSize;
      const float wy = base_y - static_cast<float>(c) * kUnitSize;
      const float wz = base_z + chunk.heights[static_cast<std::size_t>(local_idx)];
      const float u = static_cast<float>(c) / 8.0f;
      const float v = static_cast<float>(r) / 8.0f;
      EmitVertex(local_idx, wx, wy, wz, u, v);
    }
  }

  for (int r = 0; r < kInnerGrid; ++r) {
    for (int c = 0; c < kInnerGrid; ++c) {
      const int local_idx = InnerIndex(r, c);
      const float wx = base_x - (static_cast<float>(r) + 0.5f) * kUnitSize;
      const float wy = base_y - (static_cast<float>(c) + 0.5f) * kUnitSize;
      const float wz = base_z + chunk.heights[static_cast<std::size_t>(local_idx)];
      const float u = (static_cast<float>(c) + 0.5f) / 8.0f;
      const float v = (static_cast<float>(r) + 0.5f) / 8.0f;
      EmitVertex(local_idx, wx, wy, wz, u, v);
    }
  }
}

static void AppendChunkIndices(std::vector<uint16_t> &indices, const uint16_t vertex_base,
                               const TerrainChunk &chunk) {

  indices.reserve(indices.size() + 768u);
  for (int r = 0; r < kInnerGrid; ++r) {
    for (int c = 0; c < kInnerGrid; ++c) {
      if (IsHole(chunk.holes, r, c)) {
        continue;
      }

      const auto tl = static_cast<uint16_t>(vertex_base + OuterIndex(r, c));
      const auto tr = static_cast<uint16_t>(vertex_base + OuterIndex(r, c + 1));
      const auto bl = static_cast<uint16_t>(vertex_base + OuterIndex(r + 1, c));
      const auto br = static_cast<uint16_t>(vertex_base + OuterIndex(r + 1, c + 1));
      const auto ct = static_cast<uint16_t>(vertex_base + InnerIndex(r, c));

      indices.push_back(bl);
      indices.push_back(ct);
      indices.push_back(tl);

      indices.push_back(ct);
      indices.push_back(tr);
      indices.push_back(tl);

      indices.push_back(ct);
      indices.push_back(bl);
      indices.push_back(br);

      indices.push_back(ct);
      indices.push_back(br);
      indices.push_back(tr);
    }
  }
}

template <typename MaterialT>
void PopulateChunkMaterial(const AdtFile &adt, const TerrainChunk &chunk, MaterialT &material) {
  material.layer_count = std::min(static_cast<int>(chunk.layers.size()), kMaxTerrainLayers);
  for (int layer = 0; layer < material.layer_count; ++layer) {
    const auto &source = chunk.layers[static_cast<std::size_t>(layer)];
    material.layer_flags[layer] = source.flags;
    if (source.texture_id < adt.textures.size()) {
      material.texture_paths[layer] = adt.textures[source.texture_id];
    }
    if (source.texture_id < adt.texture_flags.size()) {
      material.texture_flags[layer] = adt.texture_flags[source.texture_id];
    }
  }
}

static bool IsRgbaAlphaViewValid(const std::size_t dimension, const std::size_t rgba_size,
                                 const std::size_t pixel_stride,
                                 const std::size_t row_stride) {
  if (dimension == 0u || dimension > kAlphaMapSize || rgba_size < 4u ||
      pixel_stride < 4u || row_stride == 0u || pixel_stride > row_stride / dimension) {
    return false;
  }
  const std::size_t last_coordinate = dimension - 1u;
  // An atlas subview ends at its last pixel, without trailing row padding.
  const std::size_t last_pixel_start = rgba_size - 4u;
  if (last_coordinate > last_pixel_start / row_stride) {
    return false;
  }
  const std::size_t last_row = last_coordinate * row_stride;
  return last_coordinate <= (last_pixel_start - last_row) / pixel_stride;
}

static void LogChunkAlphaFailure(const TerrainChunk &chunk, const std::uint32_t tile_x,
                                 const std::uint32_t tile_y, const char *stage,
                                 const std::string &reason) {
  diagnostics::Log(
      diagnostics::LogLevel::kWarn,
      "Terrain alpha preparation stage=" + std::string(stage) + " tile=(" +
          std::to_string(tile_x) + "," + std::to_string(tile_y) + ") chunk=(" +
          std::to_string(chunk.header.index_x) + "," +
          std::to_string(chunk.header.index_y) + ") position=(" +
          std::to_string(chunk.header.position_x) + "," +
          std::to_string(chunk.header.position_y) + ") source=ADT result=partial reason=" +
          reason);
}

static void DecodeChunkAlphaMap(const TerrainChunk &chunk, const int layer_count,
                                const std::uint32_t tile_x, const std::uint32_t tile_y,
                                const bool big_alpha, std::uint8_t *rgba,
                                const std::size_t rgba_size, const std::size_t pixel_stride,
                                const std::size_t row_stride) {
  if (rgba == nullptr ||
      !IsRgbaAlphaViewValid(kAlphaMapSize, rgba_size, pixel_stride, row_stride)) {
    LogChunkAlphaFailure(chunk, tile_x, tile_y, "decode",
                         "invalid RGBA view bytes=" + std::to_string(rgba_size) +
                             " pixel_stride=" + std::to_string(pixel_stride) +
                             " row_stride=" + std::to_string(row_stride));
    return;
  }

  for (int layer = 1; layer < layer_count; ++layer) {
    const auto &ly = chunk.layers[static_cast<std::size_t>(layer)];
    if ((ly.flags & AlphaMapFlags::kHasAlpha) == 0u) {
      continue;
    }
    const uint32_t alpha_offset = ly.alpha_map_offset;
    const auto log_layer_failure = [&](const std::string &reason) {
      LogChunkAlphaFailure(
          chunk, tile_x, tile_y, "decode",
          reason + " layer=" + std::to_string(layer) +
              " texture_id=" + std::to_string(ly.texture_id) +
              " offset=" + std::to_string(alpha_offset) +
              " flags=" + std::to_string(ly.flags) +
              " big_alpha=" + std::to_string(big_alpha) +
              " mcal_bytes=" + std::to_string(chunk.alpha_data.size()));
    };
    if (alpha_offset >= chunk.alpha_data.size()) {
      log_layer_failure("MCAL offset outside payload");
      continue;
    }

    std::size_t alpha_end = chunk.alpha_data.size();
    for (const auto &other : chunk.layers) {
      if ((other.flags & AlphaMapFlags::kHasAlpha) != 0u &&
          other.alpha_map_offset > alpha_offset) {
        alpha_end = std::min(alpha_end, static_cast<std::size_t>(other.alpha_map_offset));
      }
    }
    const std::size_t channel = static_cast<std::size_t>(layer - 1);
    if (!DecompressAlphaMapInto(
            chunk.alpha_data.data() + alpha_offset, alpha_end - alpha_offset, ly.flags,
            big_alpha,
            (chunk.header.flags & data::terrain::McnkFlags::kDoNotFixAlphaMap) == 0u,
            rgba + channel, rgba_size - channel, pixel_stride, row_stride)) {
      log_layer_failure("incomplete alpha map layer_bytes=" +
                        std::to_string(alpha_end - alpha_offset));
      for (std::size_t row = 0u; row < kAlphaMapSize; ++row) {
        for (std::size_t column = 0u; column < kAlphaMapSize; ++column) {
          rgba[row * row_stride + column * pixel_stride + channel] = 0u;
        }
      }
    }
  }

  constexpr std::size_t kShadowBytesPerRow = kAlphaMapSize / 8u;
  const bool has_shadow = chunk.shadow_map.size() >= kAlphaMapSize * kShadowBytesPerRow;
  if (!chunk.shadow_map.empty() && !has_shadow) {
    LogChunkAlphaFailure(chunk, tile_x, tile_y, "shadow",
                         "incomplete MCSH payload bytes=" +
                             std::to_string(chunk.shadow_map.size()));
  }
  for (std::size_t row = 0u; row < kAlphaMapSize; ++row) {
    for (std::size_t column = 0u; column < kAlphaMapSize; ++column) {
      auto *const pixel = rgba + row * row_stride + column * pixel_stride;
      pixel[3] = has_shadow ? ResolveRetailTerrainShadowVisibilityByte(
                                  chunk.shadow_map[row * kShadowBytesPerRow + column / 8u],
                                  static_cast<std::uint8_t>(column & 7u))
                            : 255u;
    }
  }
}

PreparedTerrainTile PrepareAdtTerrainTile(const AdtFile &adt, const uint32_t tile_x,
                                          const uint32_t tile_y, const bool big_alpha) {

  PreparedTerrainTile prepared;
  prepared.vertices.reserve(static_cast<std::size_t>(kTotalChunks * kVerticesPerChunk));
  const std::size_t holed_chunk_count = static_cast<std::size_t>(
      std::count_if(adt.chunks.begin(), adt.chunks.end(),
                    [](const TerrainChunk &chunk) { return chunk.holes != 0u; }));
  prepared.hole_indices.reserve(holed_chunk_count * 768u);
  prepared.alpha_atlas_rgba.assign(
      static_cast<std::size_t>(kTerrainAlphaAtlasSize) * kTerrainAlphaAtlasSize * 4u, 0u);
  for (std::size_t alpha = 3u; alpha < prepared.alpha_atlas_rgba.size(); alpha += 4u) {
    prepared.alpha_atlas_rgba[alpha] = 255u;
  }

  constexpr std::size_t kAtlasRowStride = static_cast<std::size_t>(kTerrainAlphaAtlasSize) * 4u;
  for (int y = 0; y < kChunksPerSide; ++y) {
    for (int x = 0; x < kChunksPerSide; ++x) {
      const std::size_t chunk_index = static_cast<std::size_t>(y * kChunksPerSide + x);
      const auto &source = adt.chunks[chunk_index];
      auto &chunk = prepared.chunks[chunk_index];
      chunk.chunk_x = static_cast<uint32_t>(x);
      chunk.chunk_y = static_cast<uint32_t>(y);
      PopulateChunkMaterial(adt, source, chunk);
      prepared.has_alpha_layers = prepared.has_alpha_layers || chunk.layer_count > 0;

      const std::size_t alpha_offset =
          (static_cast<std::size_t>(y * kAlphaMapSize) * kTerrainAlphaAtlasSize +
           static_cast<std::size_t>(x * kAlphaMapSize)) *
          4u;
      DecodeChunkAlphaMap(source, chunk.layer_count, tile_x, tile_y, big_alpha,
                          prepared.alpha_atlas_rgba.data() + alpha_offset,
                          prepared.alpha_atlas_rgba.size() - alpha_offset, 4u,
                          kAtlasRowStride);

      if (source.holes != 0u) {
        chunk.hole_index_start = static_cast<uint32_t>(prepared.hole_indices.size());
        AppendChunkIndices(prepared.hole_indices, 0u, source);
        chunk.hole_index_count =
            static_cast<uint32_t>(prepared.hole_indices.size() - chunk.hole_index_start);
        if (chunk.hole_index_count == 0u) {
          continue;
        }
      }
      InitializeBounds(chunk.bounds_min, chunk.bounds_max);
      chunk.vertex_start = static_cast<uint32_t>(prepared.vertices.size());
      AppendChunkVertices(prepared.vertices, chunk.bounds_min, chunk.bounds_max, source,
                          chunk.chunk_x, chunk.chunk_y);
      chunk.vertex_count = kVerticesPerChunk;
      chunk.valid = true;
    }
  }
  return prepared;
}

}
