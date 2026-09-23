#include "openwow/foundation/math/cosine_ease.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using openwow::math::CosineEaseInOut;

// CosineEaseInOut(t) feeds t directly into the same polynomial
// FastCosApprox uses internally *after* FastCosApprox has already divided
// radians by pi -- i.e. this function's own `t` parameter is implicitly in
// those "already-divided-by-pi" units, not radians. That happens to line
// up exactly with the conventional ease-in-out contract of t spanning
// [0, 1] over the animation, verified by hand below: at t=0/0.5/1 the
// underlying polynomial is exact (no approximation error), matching the
// standard 0.5*(1-cos(pi*t)) ease curve at those three points.
TEST_CASE("CosineEaseInOut is 0 at t=0, 0.5 at t=0.5, and 1 at t=1", "[math][ease]") {
  CHECK(CosineEaseInOut(0.0f) == Approx(0.0f).margin(0.0001));
  CHECK(CosineEaseInOut(0.5f) == Approx(0.5f).margin(0.0001));
  CHECK(CosineEaseInOut(1.0f) == Approx(1.0f).margin(0.0001));
}

TEST_CASE("CosineEaseInOut is monotonically non-decreasing across [0, 1]", "[math][ease]") {
  float previous = CosineEaseInOut(0.0f);
  for (int i = 1; i <= 10; ++i) {
    const float t = static_cast<float>(i) / 10.0f;
    const float value = CosineEaseInOut(t);
    CAPTURE(t);
    CHECK(value >= previous - 0.0005f);
    previous = value;
  }
}

TEST_CASE("CosineEaseInOut is point-symmetric about t=0.5", "[math][ease]") {
  for (const float t : {0.1f, 0.25f, 0.4f}) {
    CAPTURE(t);
    CHECK(CosineEaseInOut(0.5f - t) == Approx(1.0f - CosineEaseInOut(0.5f + t)).margin(0.001));
  }
}
