#include "openwow/game/c_input_control.h"

#include <catch2/catch_test_macros.hpp>

using openwow::game::StartsMouseSteer;

TEST_CASE("StartsMouseSteer detects the start of mouse steering",
          "[input][mouse_steer]") {
  constexpr std::uint32_t kTurnOrAction = 0x00000001u;    // right button
  constexpr std::uint32_t kCameraOrSelect = 0x00000002u;  // left button
  constexpr std::uint32_t kMoveAndSteer = 0x02000000u;    // mouselook

  CHECK(StartsMouseSteer(0u, kTurnOrAction));
  // Left first, then right: steering starts with the right button.
  CHECK(StartsMouseSteer(kCameraOrSelect, kCameraOrSelect | kTurnOrAction));
  // Right first, then left: already steering.
  CHECK_FALSE(
      StartsMouseSteer(kTurnOrAction, kTurnOrAction | kCameraOrSelect));
  // Left alone only orbits the camera.
  CHECK_FALSE(StartsMouseSteer(0u, kCameraOrSelect));
  CHECK(StartsMouseSteer(0u, kMoveAndSteer));
  CHECK_FALSE(StartsMouseSteer(kMoveAndSteer, kMoveAndSteer | kTurnOrAction));
  CHECK_FALSE(StartsMouseSteer(kTurnOrAction, 0u));
}
