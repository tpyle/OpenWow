#include "openwow/foundation/math/vec2_normalize_if_length_squared_exceeds_client_epsilon.h"
#include "openwow/foundation/math/vec3_add.h"
#include "openwow/foundation/math/vec3_cross.h"
#include "openwow/foundation/math/vec3_cross_with_xy_plane_vector.h"
#include "openwow/foundation/math/vec3_divide.h"
#include "openwow/foundation/math/vec3_exact_compare.h"
#include "openwow/foundation/math/vec3_negate.h"
#include "openwow/foundation/math/vec3_normalize.h"
#include "openwow/foundation/math/vec3_normalize_if_length_squared_exceeds_client_epsilon.h"
#include "openwow/foundation/math/vec3_scale.h"
#include "openwow/foundation/math/vec3_snap_to_zero.h"
#include "openwow/foundation/math/vec3_subtract.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

using Catch::Approx;
namespace vec3 = openwow::math::vec3;
namespace vec2 = openwow::math::vec2;

TEST_CASE("vec3::AddInPlace adds componentwise and returns the mutated pointer", "[math][vec3]") {
  float a[3] = {1.0f, 2.0f, 3.0f};
  const float b[3] = {10.0f, 20.0f, 30.0f};
  CHECK(vec3::AddInPlace(a, b) == a);
  CHECK(a[0] == Approx(11.0f));
  CHECK(a[1] == Approx(22.0f));
  CHECK(a[2] == Approx(33.0f));
}

TEST_CASE("vec3::Subtract and SubtractInPlace agree", "[math][vec3]") {
  const float lhs[3] = {5.0f, 5.0f, 5.0f};
  const float rhs[3] = {1.0f, 2.0f, 3.0f};

  float out[3] = {};
  CHECK(vec3::Subtract(out, lhs, rhs) == out);
  CHECK(out[0] == Approx(4.0f));
  CHECK(out[1] == Approx(3.0f));
  CHECK(out[2] == Approx(2.0f));

  float in_place[3] = {5.0f, 5.0f, 5.0f};
  vec3::SubtractInPlace(in_place, rhs);
  CHECK(in_place[0] == Approx(out[0]));
  CHECK(in_place[1] == Approx(out[1]));
  CHECK(in_place[2] == Approx(out[2]));
}

TEST_CASE("vec3::Cross matches the standard right-handed cross product", "[math][vec3]") {
  const float x_axis[3] = {1.0f, 0.0f, 0.0f};
  const float y_axis[3] = {0.0f, 1.0f, 0.0f};
  const float z_axis[3] = {0.0f, 0.0f, 1.0f};

  float out[3] = {};
  vec3::Cross(out, x_axis, y_axis);
  CHECK(out[0] == Approx(z_axis[0]));
  CHECK(out[1] == Approx(z_axis[1]));
  CHECK(out[2] == Approx(z_axis[2]));
}

TEST_CASE("vec3::Cross is anticommutative", "[math][vec3]") {
  const float a[3] = {1.0f, 2.0f, 3.0f};
  const float b[3] = {4.0f, -5.0f, 6.0f};

  float ab[3] = {};
  float ba[3] = {};
  vec3::Cross(ab, a, b);
  vec3::Cross(ba, b, a);
  CHECK(ab[0] == Approx(-ba[0]));
  CHECK(ab[1] == Approx(-ba[1]));
  CHECK(ab[2] == Approx(-ba[2]));
}

TEST_CASE("vec3::CrossWithXYPlaneVector matches a generic 3D cross with an implicit z=0",
          "[math][vec3]") {
  const float lhs[3] = {1.0f, 2.0f, 3.0f};
  const float rhs_xy[2] = {4.0f, -5.0f};
  const float rhs_xyz[3] = {rhs_xy[0], rhs_xy[1], 0.0f};

  float specialized[3] = {};
  float generic[3] = {};
  vec3::CrossWithXYPlaneVector(specialized, lhs, rhs_xy);
  vec3::Cross(generic, lhs, rhs_xyz);

  CHECK(specialized[0] == Approx(generic[0]));
  CHECK(specialized[1] == Approx(generic[1]));
  CHECK(specialized[2] == Approx(generic[2]));
}

TEST_CASE("vec3::DivideInPlace multiplies by the reciprocal", "[math][vec3]") {
  float a[3] = {10.0f, 20.0f, 30.0f};
  vec3::DivideInPlace(a, 2.0f);
  CHECK(a[0] == Approx(5.0f));
  CHECK(a[1] == Approx(10.0f));
  CHECK(a[2] == Approx(15.0f));
}

TEST_CASE("vec3::DivideInPlace by zero produces inf/nan rather than crashing", "[math][vec3]") {
  // No zero-guard exists in the implementation -- this test documents the
  // current (unsafe) behavior rather than asserting it is desirable. See
  // the flagged-for-future-work list: callers must guard division by zero
  // themselves.
  float positive[3] = {1.0f, 0.0f, -1.0f};
  vec3::DivideInPlace(positive, 0.0f);
  CHECK(std::isinf(positive[0]));
  CHECK(std::isnan(positive[1])); // 0 * inf
  CHECK(std::isinf(positive[2]));
}

TEST_CASE("vec3::CopyNegated negates each component", "[math][vec3]") {
  const float src[3] = {1.0f, -2.0f, 0.0f};
  float out[3] = {};
  CHECK(vec3::CopyNegated(out, src) == out);
  CHECK(out[0] == Approx(-1.0f));
  CHECK(out[1] == Approx(2.0f));
  CHECK(out[2] == Approx(0.0f));
}

TEST_CASE("vec3::ScaleInPlace multiplies each component", "[math][vec3]") {
  float a[3] = {1.0f, -2.0f, 3.0f};
  vec3::ScaleInPlace(a, -2.0f);
  CHECK(a[0] == Approx(-2.0f));
  CHECK(a[1] == Approx(4.0f));
  CHECK(a[2] == Approx(-6.0f));
}

TEST_CASE("vec3::AnyComponentDiffers / AllComponentsEqual are exact bitwise comparisons",
          "[math][vec3]") {
  const float a[3] = {1.0f, 2.0f, 3.0f};
  const float b[3] = {1.0f, 2.0f, 3.0f};
  // 3.001f, not 3.0000001f: the latter is below float's precision at this
  // magnitude and rounds to the exact same bit pattern as 3.0f, which
  // would make this "differs" case accidentally equal instead of testing
  // the intended near-but-not-exactly-equal scenario.
  const float c[3] = {1.0f, 2.0f, 3.001f};

  CHECK(vec3::AllComponentsEqual(a, b));
  CHECK_FALSE(vec3::AnyComponentDiffers(a, b));

  CHECK_FALSE(vec3::AllComponentsEqual(a, c));
  CHECK(vec3::AnyComponentDiffers(a, c));
}

TEST_CASE("vec3::NormalizeInPlace produces a unit vector", "[math][vec3]") {
  float v[3] = {3.0f, 0.0f, 4.0f}; // length 5
  vec3::NormalizeInPlace(v);
  const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  CHECK(length == Approx(1.0f));
  CHECK(v[0] == Approx(0.6f));
  CHECK(v[2] == Approx(0.8f));
}

TEST_CASE("vec3::NormalizeInPlace on a zero vector produces nan (no guard)", "[math][vec3]") {
  float zero[3] = {0.0f, 0.0f, 0.0f};
  vec3::NormalizeInPlace(zero);
  CHECK(std::isnan(zero[0]));
  CHECK(std::isnan(zero[1]));
  CHECK(std::isnan(zero[2]));
}

TEST_CASE("vec3::NormalizeInPlaceIfLengthSquaredExceedsClientEpsilon is a guarded variant of "
          "NormalizeInPlace",
          "[math][vec3]") {
  SECTION("above epsilon: normalizes just like the unguarded version") {
    float v[3] = {3.0f, 0.0f, 4.0f};
    vec3::NormalizeInPlaceIfLengthSquaredExceedsClientEpsilon(v);
    CHECK(v[0] == Approx(0.6f));
    CHECK(v[2] == Approx(0.8f));
  }

  SECTION("at/below epsilon: left completely untouched, unlike the unguarded version") {
    float tiny[3] = {1e-5f, 0.0f, 0.0f};
    const float original[3] = {tiny[0], tiny[1], tiny[2]};
    vec3::NormalizeInPlaceIfLengthSquaredExceedsClientEpsilon(tiny);
    CHECK(tiny[0] == original[0]);
    CHECK(tiny[1] == original[1]);
    CHECK(tiny[2] == original[2]);
  }

  SECTION("exact zero vector: also left untouched (would have produced NaN unguarded)") {
    float zero[3] = {0.0f, 0.0f, 0.0f};
    vec3::NormalizeInPlaceIfLengthSquaredExceedsClientEpsilon(zero);
    CHECK(zero[0] == 0.0f);
    CHECK(zero[1] == 0.0f);
    CHECK(zero[2] == 0.0f);
  }
}

TEST_CASE("vec3::SnapToZero zeroes only components within epsilon of zero", "[math][vec3]") {
  float v[3] = {1e-9f, 1.0f, -1e-9f};
  vec3::SnapToZero(v);
  CHECK(v[0] == 0.0f);
  CHECK(v[1] == Approx(1.0f));
  CHECK(v[2] == 0.0f);
}

TEST_CASE("vec3::SnapToZero leaves values at/above the epsilon threshold untouched",
          "[math][vec3]") {
  float v[3] = {1e-7f, -1e-7f, 0.0f};
  const float original[3] = {v[0], v[1], v[2]};
  vec3::SnapToZero(v);
  CHECK(v[0] == original[0]);
  CHECK(v[1] == original[1]);
  CHECK(v[2] == original[2]);
}

TEST_CASE("vec3::SnapToZero does not flip the sign of negative zero", "[math][vec3]") {
  // The implementation explicitly skips a component that is already
  // exactly 0.0f -- and per IEEE 754, `0.0f != -0.0f` is false, so a
  // -0.0f component takes the same "already zero" early-out and is left
  // as -0.0f rather than being snapped to +0.0f.
  float v[3] = {-0.0f, 0.0f, 0.0f};
  vec3::SnapToZero(v);
  CHECK(std::signbit(v[0]));
  CHECK_FALSE(std::signbit(v[1]));
}

TEST_CASE(
    "vec2::NormalizeInPlaceIfLengthSquaredExceedsClientEpsilon mirrors the vec3 guarded variant",
    "[math][vec2]") {
  SECTION("above epsilon: normalizes to unit length") {
    float xy[2] = {3.0f, 4.0f}; // length 5
    vec2::NormalizeInPlaceIfLengthSquaredExceedsClientEpsilon(xy);
    CHECK(xy[0] == Approx(0.6f));
    CHECK(xy[1] == Approx(0.8f));
  }

  SECTION("at/below epsilon: left untouched") {
    float tiny[2] = {1e-5f, 0.0f};
    const float original[2] = {tiny[0], tiny[1]};
    vec2::NormalizeInPlaceIfLengthSquaredExceedsClientEpsilon(tiny);
    CHECK(tiny[0] == original[0]);
    CHECK(tiny[1] == original[1]);
  }
}
