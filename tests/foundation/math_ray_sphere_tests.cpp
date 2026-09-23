#include "openwow/foundation/math/ray_aabb_intersect.h"
#include "openwow/foundation/math/sphere_aabb_2d.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using openwow::math::PointAABBDistanceTest2D;
using openwow::math::RayAABBIntersect;
using openwow::math::RayIntersectAABB;
using openwow::math::SphereAABBTest2D;

// --- ray_aabb_intersect.h -----------------------------------------------

TEST_CASE("RayAABBIntersect: origin already inside the box hits immediately at t=0",
          "[math][ray]") {
  const float origin[3] = {5.0f, 5.0f, 5.0f};
  const float dir[3] = {1.0f, 0.0f, 0.0f};
  const float box_min[3] = {0.0f, 0.0f, 0.0f};
  const float box_max[3] = {10.0f, 10.0f, 10.0f};

  float t = -1.0f;
  float point[3] = {};
  REQUIRE(RayAABBIntersect(origin, dir, box_min, box_max, &t, point));
  CHECK(t == Approx(0.0f));
  CHECK(point[0] == Approx(origin[0]));
  CHECK(point[1] == Approx(origin[1]));
  CHECK(point[2] == Approx(origin[2]));
}

TEST_CASE("RayAABBIntersect: a ray pointed at the box from outside hits its near face",
          "[math][ray]") {
  const float origin[3] = {-5.0f, 5.0f, 5.0f};
  const float dir[3] = {1.0f, 0.0f, 0.0f};
  const float box_min[3] = {0.0f, 0.0f, 0.0f};
  const float box_max[3] = {10.0f, 10.0f, 10.0f};

  float t = -1.0f;
  float point[3] = {};
  REQUIRE(RayAABBIntersect(origin, dir, box_min, box_max, &t, point));
  CHECK(t == Approx(5.0f));
  CHECK(point[0] == Approx(0.0f));
  CHECK(point[1] == Approx(5.0f));
  CHECK(point[2] == Approx(5.0f));
}

TEST_CASE("RayAABBIntersect: a ray pointed away from the box misses", "[math][ray]") {
  const float origin[3] = {-5.0f, 5.0f, 5.0f};
  const float dir[3] = {-1.0f, 0.0f, 0.0f}; // moving further away
  const float box_min[3] = {0.0f, 0.0f, 0.0f};
  const float box_max[3] = {10.0f, 10.0f, 10.0f};

  float t = 0.0f;
  float point[3] = {};
  CHECK_FALSE(RayAABBIntersect(origin, dir, box_min, box_max, &t, point));
}

TEST_CASE("RayAABBIntersect: a parallel ray that never reaches the box on another axis misses",
          "[math][ray]") {
  // Ray travels along +x but starts above the box on y, with zero y
  // velocity -- it can never enter the box.
  const float origin[3] = {-5.0f, 20.0f, 5.0f};
  const float dir[3] = {1.0f, 0.0f, 0.0f};
  const float box_min[3] = {0.0f, 0.0f, 0.0f};
  const float box_max[3] = {10.0f, 10.0f, 10.0f};

  float t = 0.0f;
  float point[3] = {};
  CHECK_FALSE(RayAABBIntersect(origin, dir, box_min, box_max, &t, point));
}

TEST_CASE("RayIntersectAABB matches RayAABBIntersect via the packed ray/aabb layout",
          "[math][ray]") {
  const float ray[6] = {-5.0f, 5.0f, 5.0f, 1.0f, 0.0f, 0.0f};   // origin, dir
  const float box[6] = {0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f}; // min, max

  float t = -1.0f;
  float point[3] = {};
  REQUIRE(RayIntersectAABB(ray, box, &t, point));
  CHECK(t == Approx(5.0f));
  CHECK(point[0] == Approx(0.0f));
}

TEST_CASE("RayIntersectAABB tolerates null outT/outPoint by using internal fallbacks",
          "[math][ray]") {
  const float ray[6] = {5.0f, 5.0f, 5.0f, 1.0f, 0.0f, 0.0f}; // origin already inside
  const float box[6] = {0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f};
  CHECK(RayIntersectAABB(ray, box, nullptr, nullptr));
}

// --- sphere_aabb_2d.h -----------------------------------------------------

TEST_CASE(
    "PointAABBDistanceTest2D mode 3 is a standard clamped-nearest-point circle/AABB overlap test",
    "[math][sphere2d]") {
  const float aabb_min[2] = {0.0f, 0.0f};
  const float aabb_max[2] = {10.0f, 10.0f};

  SECTION("center inside the box always overlaps, even with a zero radius") {
    const float center[2] = {5.0f, 5.0f};
    CHECK(PointAABBDistanceTest2D(aabb_min, aabb_max, center, 0.0f, 3));
  }

  SECTION("far outside, radius too small to reach: no overlap") {
    const float center[2] = {100.0f, 100.0f};
    CHECK_FALSE(PointAABBDistanceTest2D(aabb_min, aabb_max, center, 1.0f, 3));
  }

  SECTION(
      "outside, but the radius exactly reaches the nearest corner: overlaps (boundary inclusive)") {
    // Nearest point to (13, 4) that's in the box is (10, 4); distance = 3.
    const float center[2] = {13.0f, 4.0f};
    CHECK(PointAABBDistanceTest2D(aabb_min, aabb_max, center, 3.0f, 3));
    CHECK_FALSE(PointAABBDistanceTest2D(aabb_min, aabb_max, center, 2.9f, 3));
  }
}

TEST_CASE("PointAABBDistanceTest2D modes 0 and 2 test whether the circle's *boundary* crosses the "
          "box, not simple overlap",
          "[math][sphere2d]") {
  // Unlike mode 3 (any overlap of the filled disk), modes 0 and 2 require
  // min_dist_sq <= r^2 <= max_dist_sq -- i.e. the box must straddle the
  // circle's boundary. A box fully swallowed by the circle (every corner
  // closer than the radius) has max_dist_sq < r^2 too, so it does NOT
  // count as a hit for these modes even though it obviously overlaps.
  //
  // Mode 1 turns out NOT to share this "boundary crossing" semantic
  // despite sitting between modes 0 and 2 -- tracing its branches shows
  // it computes the same clamped-nearest-point distance as mode 3 (just
  // with a different early-exit shortcut), so it agrees with mode 3 here
  // instead of modes 0/2. This was verified empirically against the
  // actual implementation, not assumed from the mode-0/2 pattern.
  const float aabb_min[2] = {4.0f, 4.0f};
  const float aabb_max[2] = {6.0f, 6.0f}; // a small 2x2 box near the origin
  const float center[2] = {0.0f, 0.0f};
  const float radius_fully_containing = 100.0f; // every box corner well inside this radius

  for (const int mode : {0, 2}) {
    CAPTURE(mode);
    CHECK_FALSE(PointAABBDistanceTest2D(aabb_min, aabb_max, center, radius_fully_containing, mode));
  }
  for (const int mode : {1, 3}) {
    CAPTURE(mode);
    CHECK(PointAABBDistanceTest2D(aabb_min, aabb_max, center, radius_fully_containing, mode));
  }
}

TEST_CASE("PointAABBDistanceTest2D modes 0-2 agree the box is missed when it's far from the circle "
          "entirely",
          "[math][sphere2d]") {
  const float aabb_min[2] = {0.0f, 0.0f};
  const float aabb_max[2] = {10.0f, 10.0f};
  const float center[2] = {1000.0f, 1000.0f};

  for (const int mode : {0, 1, 2, 3}) {
    CAPTURE(mode);
    CHECK_FALSE(PointAABBDistanceTest2D(aabb_min, aabb_max, center, 1.0f, mode));
  }
}

TEST_CASE("PointAABBDistanceTest2D rejects an unknown mode", "[math][sphere2d]") {
  const float aabb_min[2] = {0.0f, 0.0f};
  const float aabb_max[2] = {10.0f, 10.0f};
  const float center[2] = {5.0f, 5.0f};
  CHECK_FALSE(PointAABBDistanceTest2D(aabb_min, aabb_max, center, 1.0f, 42));
}

TEST_CASE(
    "SphereAABBTest2D unpacks the 6-float AABB (XY only) and 4-float sphere (radius at index 3)",
    "[math][sphere2d]") {
  // aabb6 = {minX, minY, minZ, maxX, maxY, maxZ} but only XY is used here;
  // sphere4 = {x, y, <unused>, radius}.
  const float aabb6[6] = {0.0f, 0.0f, -999.0f, 10.0f, 10.0f, 999.0f};
  const float sphere_inside[4] = {5.0f, 5.0f, 0.0f, 1.0f};
  CHECK(SphereAABBTest2D(aabb6, sphere_inside, 3));

  const float sphere_far[4] = {1000.0f, 1000.0f, 0.0f, 1.0f};
  CHECK_FALSE(SphereAABBTest2D(aabb6, sphere_far, 3));
}
