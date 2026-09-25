#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace openwow::world {

/// Grows `bounds` ({min x, y, z, max x, y, z}) to also contain every box in
/// `parts`.
///
/// A WMO placement's box comes from placement data that isn't always right:
/// several transports have an all-zero GameObjectDisplayInfo geobox, which
/// collapsed the placement to a point at the model origin and frustum-culled
/// the whole ship while its doodads kept drawing. Covering the group boxes
/// keeps the placement box at least as large as the geometry it holds.
inline void ExpandBoundsToCover(std::array<float, 6>& bounds,
                                const std::span<const std::array<float, 6>> parts) {
  for (const std::array<float, 6>& part : parts) {
    for (std::size_t axis = 0u; axis < 3u; ++axis) {
      bounds[axis] = std::min(bounds[axis], part[axis]);
      bounds[axis + 3u] = std::max(bounds[axis + 3u], part[axis + 3u]);
    }
  }
}

}  // namespace openwow::world
