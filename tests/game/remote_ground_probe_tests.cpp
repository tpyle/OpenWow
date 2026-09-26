#include "openwow/game/objects/unit/remote_ground_probe.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace probe = openwow::game::remote_ground_probe;

namespace {
constexpr float kHumanoidHeight = 2.0277777f;
}

TEST_CASE("Unseeded probe reaches ground well below a spawn height",
          "[unit][ground_probe]") {
  // Darnassus Sentinel spawned 1.71 yd above the terrain.
  const float server_z = 1318.16f;
  const float terrain_z = 1316.45f;
  const auto window = probe::ComputeWindow({9941.64f, 2615.43f, server_z}, false,
                                           {}, 0.0f, kHumanoidHeight);
  CHECK_FALSE(window.seeded);
  CHECK(window.origin_z == Catch::Approx(server_z + 0.5f));
  CHECK(window.distance == Catch::Approx(0.5f + 2.0f * kHumanoidHeight));
  CHECK(window.origin_z - window.distance < terrain_z);
  // Never starts above the server height by more than the seed allowance,
  // so a unit can't be lifted to a floor above it.
  CHECK(window.origin_z <= server_z + probe::kSeedVerticalAllowance);
}

TEST_CASE("Seeded probe follows the previous ground", "[unit][ground_probe]") {
  const std::array<float, 3> anchor{100.0f, 100.0f, 50.0f};
  SECTION("standing still") {
    const auto window =
        probe::ComputeWindow({100.0f, 100.0f, 51.7f}, true, anchor, 51.7f,
                             kHumanoidHeight);
    CHECK(window.seeded);
    CHECK(window.origin_z == Catch::Approx(50.5f));
    CHECK(window.distance == Catch::Approx(1.0f));
  }
  SECTION("slack grows with planar travel") {
    const auto window =
        probe::ComputeWindow({100.2f, 100.0f, 51.7f}, true, anchor, 51.7f,
                             kHumanoidHeight);
    const float allowance = 0.25f + 0.2f * probe::kRisePerPlanarUnit;
    CHECK(window.seeded);
    // 100.2 - 100 isn't exactly 0.2 in single precision.
    CHECK(window.origin_z == Catch::Approx(50.0f + allowance).margin(1e-3));
    CHECK(window.distance == Catch::Approx(2.0f * allowance).margin(1e-3));
  }
}

TEST_CASE("Probe starts over after a relocation", "[unit][ground_probe]") {
  const std::array<float, 3> anchor{100.0f, 100.0f, 50.0f};
  SECTION("planar jump") {
    const auto window = probe::ComputeWindow({110.0f, 100.0f, 51.7f}, true,
                                             anchor, 51.7f, kHumanoidHeight);
    CHECK_FALSE(window.seeded);
    CHECK(window.origin_z == Catch::Approx(52.2f));
  }
  SECTION("vertical jump beyond the slack") {
    const auto window = probe::ComputeWindow({100.0f, 100.0f, 60.0f}, true,
                                             anchor, 51.7f, kHumanoidHeight);
    CHECK_FALSE(window.seeded);
    CHECK(window.origin_z == Catch::Approx(60.5f));
  }
  SECTION("negative collision height is ignored") {
    const auto window =
        probe::ComputeWindow({0.0f, 0.0f, 10.0f}, false, {}, 0.0f, -3.0f);
    CHECK(window.distance == Catch::Approx(0.5f));
  }
}
