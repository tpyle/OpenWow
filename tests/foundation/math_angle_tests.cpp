#include "openwow/foundation/math/angle_normalize.h"
#include "openwow/foundation/math/clamp_pitch.h"
#include "openwow/foundation/math/float_compare.h"
#include "openwow/foundation/math/planar_facing_angle.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
using namespace openwow::math;

TEST_CASE("NormalizeSignedAngle maps into (-pi, pi], inclusive at both ends", "[math][angle]") {
  CHECK(NormalizeSignedAngle(0.0f) == Approx(0.0f));
  CHECK(NormalizeSignedAngle(kAnglePi) == Approx(kAnglePi));
  CHECK(NormalizeSignedAngle(-kAnglePi) == Approx(-kAnglePi));
}

TEST_CASE("NormalizeSignedAngle wraps values outside one revolution", "[math][angle]") {
  CHECK(NormalizeSignedAngle(kAngleTwoPi + 0.5f) == Approx(0.5f).margin(0.0001));
  // 3*pi mod 2*pi == pi mathematically, and +pi is already inside the
  // inclusive (-pi, pi] range, so no further adjustment happens -- it
  // does NOT wrap down to -pi. Mirrored for the negative case: -3*pi
  // reduces to -pi, not +pi.
  CHECK(NormalizeSignedAngle(3.0f * kAnglePi) == Approx(kAnglePi).margin(0.0001));
  CHECK(NormalizeSignedAngle(-3.0f * kAnglePi) == Approx(-kAnglePi).margin(0.0001));
}

TEST_CASE("NormalizePositiveAngle maps into [0, 2*pi)", "[math][angle]") {
  CHECK(NormalizePositiveAngle(0.0f) == Approx(0.0f));
  CHECK(NormalizePositiveAngle(-0.5f) == Approx(kAngleTwoPi - 0.5f).margin(0.0001));
  CHECK(NormalizePositiveAngle(kAngleTwoPi + 1.0f) == Approx(1.0f).margin(0.0001));

  const float result = NormalizePositiveAngle(-kAngleTwoPi);
  CHECK(result >= 0.0f);
  CHECK(result < kAngleTwoPi);
}

TEST_CASE("ClampPitch passes through values already within [-pi/2, pi/2]", "[math][angle]") {
  CHECK(ClampPitch(0.0f) == Approx(0.0f));
  CHECK(ClampPitch(1.0f) == Approx(1.0f));
  CHECK(ClampPitch(-1.0f) == Approx(-1.0f));
  CHECK(ClampPitch(kPitchHalfPi) == Approx(kPitchHalfPi));
  CHECK(ClampPitch(-kPitchHalfPi) == Approx(-kPitchHalfPi));
}

TEST_CASE("ClampPitch clamps values outside [-pi/2, pi/2]", "[math][angle]") {
  CHECK(ClampPitch(kPitchHalfPi + 1.0f) == Approx(kPitchHalfPi));
  CHECK(ClampPitch(-kPitchHalfPi - 1.0f) == Approx(-kPitchHalfPi));
}

TEST_CASE("ComputeRetailPlanarFacingAngle matches atan2 in the general case",
          "[math][angle][planar]") {
  const PlanarPoint from{0.0f, 0.0f};
  const PlanarPoint to{3.0f, 4.0f};
  const float angle = ComputeRetailPlanarFacingAngle(from, to);
  CHECK(angle == Approx(std::atan2(4.0, 3.0)));
}

TEST_CASE("ComputeRetailPlanarFacingAngle: purely horizontal movement", "[math][angle][planar]") {
  SECTION("moving in +x returns 0") {
    CHECK(ComputeRetailPlanarFacingAngle({0.0f, 0.0f}, {5.0f, 0.0f}) == Approx(0.0f));
  }
  SECTION("moving in -x returns pi") {
    CHECK(ComputeRetailPlanarFacingAngle({5.0f, 0.0f}, {0.0f, 0.0f}) == Approx(kLegacyFacingPi));
  }
}

TEST_CASE("ComputeRetailPlanarFacingAngle: purely vertical movement (dx within epsilon)",
          "[math][angle][planar]") {
  SECTION("moving in +y returns pi/2") {
    CHECK(ComputeRetailPlanarFacingAngle({0.0f, 0.0f}, {0.0f, 5.0f}) ==
          Approx(0.5f * kLegacyFacingPi));
  }
  SECTION("moving in -y returns 3*pi/2") {
    CHECK(ComputeRetailPlanarFacingAngle({0.0f, 5.0f}, {0.0f, 0.0f}) ==
          Approx(1.5f * kLegacyFacingPi));
  }
}

TEST_CASE("ComputeRetailPlanarFacingAngle: coincident points default to pi/2, not 0",
          "[math][angle][planar]") {
  // Both dx and dy are exactly zero, so this hits the same "dx within
  // epsilon" branch as purely-vertical movement, and dy>=0 (dy==0 counts)
  // takes the pi/2 branch -- NOT an arbitrary/undefined angle and NOT 0.
  const PlanarPoint p{1.0f, 1.0f};
  CHECK(ComputeRetailPlanarFacingAngle(p, p) == Approx(0.5f * kLegacyFacingPi));
}

TEST_CASE("WithinTolerance/OutsideTolerance are exact complements except at the boundary",
          "[math][float_compare]") {
  using namespace openwow::math::float_compare;

  CHECK(WithinTolerance(1.0f, 1.05f, 0.1f));
  CHECK_FALSE(OutsideTolerance(1.0f, 1.05f, 0.1f));

  CHECK_FALSE(WithinTolerance(1.0f, 2.0f, 0.1f));
  CHECK(OutsideTolerance(1.0f, 2.0f, 0.1f));

  SECTION("a difference exactly equal to the tolerance counts as outside, not within") {
    CHECK_FALSE(WithinTolerance(1.0f, 1.5f, 0.5f));
    CHECK(OutsideTolerance(1.0f, 1.5f, 0.5f));
  }
}

TEST_CASE("WithinClientEpsilon/OutsideClientEpsilon delegate to the client epsilon constant",
          "[math][float_compare]") {
  using namespace openwow::math::float_compare;

  CHECK(WithinClientEpsilon(1.0f, 1.0f));
  CHECK_FALSE(OutsideClientEpsilon(1.0f, 1.0f));
  CHECK(OutsideClientEpsilon(1.0f, 1.001f));
}

TEST_CASE("WithinWideClientEpsilon uses a wider tolerance than WithinClientEpsilon",
          "[math][float_compare]") {
  using namespace openwow::math::float_compare;

  const float lhs = 1.0f;
  const float rhs = 1.0f + kClientFloatEpsilon * 2.0f;
  CHECK_FALSE(WithinClientEpsilon(lhs, rhs));
  CHECK(WithinWideClientEpsilon(lhs, rhs));
}
