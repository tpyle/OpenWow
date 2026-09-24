#pragma once

#include "openwow/render/api/math/render_math_types.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace bgfx {
struct Encoder;
}

namespace openwow::render {

inline constexpr std::size_t kWorldShadowProductCount = 4u;

enum class WorldShadowProduct : std::uint8_t {
  Center = 0,
  Near = 1,
  Mid = 2,
  Far = 3,
};

/// Moves a light view matrix's translation onto the shadow map's texel grid.
///
/// A shadow map that follows the player by fractions of a texel re-rasterises
/// every static caster at a slightly different offset each frame, so shadow
/// edges on static geometry shimmer. Snapping keeps each texel over the same
/// world position while the light direction is unchanged. `view` is a
/// row-vector (bx) matrix; `half_extent` is the orthographic half-width and
/// `resolution` the shadow map size in texels.
inline void SnapLightViewToTexelGrid(RenderMatrix4x4& view, const float half_extent,
                                     const std::uint32_t resolution) {
  if (resolution == 0u || !(half_extent > 0.0f)) {
    return;
  }
  const float texel = (2.0f * half_extent) / static_cast<float>(resolution);
  // The translation row is the light-space position of the world origin.
  view[12] = std::round(view[12] / texel) * texel;
  view[13] = std::round(view[13] / texel) * texel;
}

class ShadowRenderData {
 public:
  ShadowRenderData();
  ~ShadowRenderData();

  ShadowRenderData(const ShadowRenderData&) = delete;
  ShadowRenderData& operator=(const ShadowRenderData&) = delete;

  void Configure(std::uint8_t quality, std::uint16_t resolution);
  [[nodiscard]] std::uint8_t quality() const noexcept { return quality_; }
  [[nodiscard]] std::uint16_t resolution() const noexcept {
    return resolution_;
  }
  [[nodiscard]] std::size_t active_product_count() const noexcept;

  [[nodiscard]] bool CreateResources();
  void DestroyResources();
  [[nodiscard]] bool resources_valid() const noexcept;

  void SetLightDirection(const RenderVec3& surface_to_light) noexcept;
  [[nodiscard]] bool PrepareProduct(std::size_t product_index,
                                    const RenderVec3& target);
  /// Clears the 1x1 fallback depth map (sampled for shadow slots without a
  /// published map) to depth 1.0, i.e. fully lit, the first time it is
  /// called after the resources are created. `view_id` must be a view no
  /// other pass uses; it is only touched on that one frame.
  void ClearFallbackDepthOnce(std::uint8_t view_id);
  void BeginShadowDepthPass(std::size_t product_index,
                            std::uint8_t view_id) const;

  [[nodiscard]] const float* light_view(std::size_t product_index) const;
  [[nodiscard]] const float* light_projection(std::size_t product_index) const;
  [[nodiscard]] RenderVec3 product_target(std::size_t product_index) const;
  [[nodiscard]] float product_half_extent(std::size_t product_index) const;

  void SetPublishedProductCount(std::size_t count) noexcept;
  [[nodiscard]] std::size_t published_product_count() const noexcept {
    return published_product_count_;
  }

  void BindTerrainShadowState(bgfx::Encoder* encoder = nullptr) const;
  void BindModelShadowState(bool enabled,
                            bgfx::Encoder* encoder = nullptr) const;

 private:
  struct BackendResources;

  void BindReceiverState(std::size_t first_product, bool enabled,
                         bgfx::Encoder* encoder) const;

  std::unique_ptr<BackendResources> backend_;
  std::array<RenderMatrix4x4, kWorldShadowProductCount> light_views_{};
  std::array<RenderMatrix4x4, kWorldShadowProductCount> light_projections_{};
  std::array<RenderMatrix4x4, kWorldShadowProductCount> receiver_matrices_{};
  std::array<RenderVec3, kWorldShadowProductCount> product_targets_{};
  RenderVec3 surface_to_light_{0.0f, -1.0f, 0.0f};
  std::uint8_t quality_{0u};
  std::uint16_t resolution_{1024u};
  std::size_t published_product_count_{0u};
  bool fallback_cleared_{false};
};

void SetActiveWorldShadowRenderData(const ShadowRenderData* data) noexcept;
void BindActiveWorldModelShadowState(bool enabled,
                                     bgfx::Encoder* encoder = nullptr) noexcept;

}
