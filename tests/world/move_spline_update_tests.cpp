#include "openwow/world/movement/movement_spline.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>

namespace {

using openwow::game::MovementUpdate;
using openwow::game::Vec3;
using openwow::world::MoveSpline;

MovementUpdate MakeUpdate(std::uint8_t mode, std::initializer_list<Vec3> nodes) {
  MovementUpdate update{};
  update.spline.active = true;
  update.spline.duration = 4000;
  update.spline.time_passed = 0;
  update.spline.mode = mode;
  for (const Vec3& node : nodes) {
    update.spline.waypoints.push_back(node.x);
    update.spline.waypoints.push_back(node.y);
    update.spline.waypoints.push_back(node.z);
  }
  const Vec3 last = *(nodes.end() - 1);
  update.spline.dest_x = last.x;
  update.spline.dest_y = last.y;
  update.spline.dest_z = last.z;
  return update;
}

}  // namespace

TEST_CASE("Linear update splines keep every node", "[world][move_spline]") {
  MoveSpline spline;
  spline.Initialize(MakeUpdate(0, {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}}));
  REQUIRE(spline.GetPointCount() == 4);
  CHECK(spline.GetPoints().front().x == 0.0f);
  CHECK(spline.GetPoints().back().y == 10.0f);
  CHECK(spline.GetTotalArcLength() == Catch::Approx(30.0f));
}

TEST_CASE("Short linear update splines keep their middle node", "[world][move_spline]") {
  MoveSpline spline;
  spline.Initialize(MakeUpdate(0, {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}}));
  REQUIRE(spline.GetPointCount() == 3);
  CHECK(spline.GetTotalArcLength() == Catch::Approx(20.0f));
}

TEST_CASE("Catmull-Rom update splines drop their end padding", "[world][move_spline]") {
  MoveSpline spline;
  spline.Initialize(
      MakeUpdate(1, {{-10, 0, 0}, {0, 0, 0}, {10, 0, 0}, {20, 0, 0}, {30, 0, 0}}));
  REQUIRE(spline.GetPointCount() == 3);
  CHECK(spline.GetPoints().front().x == 0.0f);
  CHECK(spline.GetPoints().back().x == 20.0f);
}
