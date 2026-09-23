#include "openwow/foundation/math/vec4_componentwise_add.h"
#include "openwow/foundation/math/vec4_componentwise_divide.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
namespace vec4 = openwow::math::vec4;

TEST_CASE("vec4::ComponentwiseAdd adds each of the four components", "[math][vec4]") {
  const float lhs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  const float rhs[4] = {10.0f, 20.0f, 30.0f, 40.0f};
  float dest[4] = {};
  CHECK(vec4::ComponentwiseAdd(dest, lhs, rhs) == dest);
  CHECK(dest[0] == Approx(11.0f));
  CHECK(dest[1] == Approx(22.0f));
  CHECK(dest[2] == Approx(33.0f));
  CHECK(dest[3] == Approx(44.0f));
}

TEST_CASE("vec4::ComponentwiseDivide divides each of the four components", "[math][vec4]") {
  const float lhs[4] = {10.0f, 20.0f, 30.0f, 40.0f};
  const float rhs[4] = {2.0f, 4.0f, 5.0f, 8.0f};
  float dest[4] = {};
  CHECK(vec4::ComponentwiseDivide(dest, lhs, rhs) == dest);
  CHECK(dest[0] == Approx(5.0f));
  CHECK(dest[1] == Approx(5.0f));
  CHECK(dest[2] == Approx(6.0f));
  CHECK(dest[3] == Approx(5.0f));
}

TEST_CASE("vec4::ComponentwiseDivide by a zero component produces inf/nan rather than crashing",
          "[math][vec4]") {
  // No zero-guard, same as vec3's divide -- documents the current
  // (unsafe) behavior rather than asserting it is desirable.
  const float lhs[4] = {1.0f, 0.0f, -1.0f, 5.0f};
  const float rhs[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float dest[4] = {};
  vec4::ComponentwiseDivide(dest, lhs, rhs);
  CHECK(std::isinf(dest[0]));
  CHECK(std::isnan(dest[1])); // 0 / 0
  CHECK(std::isinf(dest[2]));
  CHECK(dest[3] == Approx(5.0f));
}
