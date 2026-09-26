#pragma once

#include <algorithm>
#include <array>
#include <cmath>

/// Where to search for the ground under a server-positioned (remote) unit.
namespace openwow::game::remote_ground_probe {

inline constexpr float kSeedVerticalAllowance = 0.5f;
inline constexpr float kVerticalSlack = 0.25f;
inline constexpr float kRisePerPlanarUnit = 5.671282f;
inline constexpr float kMaxVerticalAllowance = 8.0f;

/// A downward ray: from `origin_z` for `distance` yards. `seeded` reports
/// whether the window follows the previously found ground (false when the
/// probe starts over from the server height).
struct Window {
  float origin_z = 0.0f;
  float distance = 0.0f;
  bool seeded = false;
};

/// Chooses the probe for a unit the server places at `server`.
///
/// With a previous ground contact (`seeded`, at `anchor`, found while the
/// server height was `anchor_server_z`), the ray follows that ground within a
/// slack that grows with planar travel. Otherwise, or after a relocation too
/// far to follow, it starts just above the server height, so it can never
/// climb to a higher floor, and reaches down twice the unit's collision
/// height: spawn heights are commonly 1-2 yd above the ground.
[[nodiscard]] inline Window ComputeWindow(const std::array<float, 3>& server,
                                          const bool seeded,
                                          const std::array<float, 3>& anchor,
                                          const float anchor_server_z,
                                          const float collision_height) {
  if (seeded) {
    const float delta_x = server[0] - anchor[0];
    const float delta_y = server[1] - anchor[1];
    const float planar_distance =
        std::sqrt(delta_x * delta_x + delta_y * delta_y);
    const float allowance =
        std::clamp(kVerticalSlack + planar_distance * kRisePerPlanarUnit,
                   kSeedVerticalAllowance, kMaxVerticalAllowance);
    if (planar_distance <= kMaxVerticalAllowance &&
        std::fabs(server[2] - anchor_server_z) <= allowance) {
      return {anchor[2] + allowance, allowance * 2.0f, true};
    }
  }
  return {server[2] + kSeedVerticalAllowance,
          kSeedVerticalAllowance + std::max(collision_height, 0.0f) * 2.0f,
          false};
}

}  // namespace openwow::game::remote_ground_probe
