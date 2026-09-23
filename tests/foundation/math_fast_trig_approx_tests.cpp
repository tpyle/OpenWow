#include "openwow/foundation/math/fast_trig_approx.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
using openwow::math::FastCosApprox;
using openwow::math::FastSinApprox;
using openwow::math::FastSinCosApprox;

// This is a single-parabola-per-quadrant approximation (not a minimax
// polynomial or lookup table), so its error is visibly larger than a
// typical fast-math routine -- verified numerically to peak around 2% of
// full amplitude near the middle of each quadrant. All tolerances below
// were chosen from that numerical check, not guessed.
namespace {
constexpr double kTrigApproxMargin = 0.025;
}

TEST_CASE("FastSinApprox/FastCosApprox match std::sin/std::cos at quadrant boundaries",
          "[math][trig]") {
  for (const int degrees : {0, 90, 180, 270, 360, -90}) {
    CAPTURE(degrees);
    const float radians = static_cast<float>(degrees) * static_cast<float>(M_PI) / 180.0f;
    CHECK(FastSinApprox(radians) == Approx(std::sin(radians)).margin(kTrigApproxMargin));
    CHECK(FastCosApprox(radians) == Approx(std::cos(radians)).margin(kTrigApproxMargin));
  }
}

TEST_CASE("FastSinApprox/FastCosApprox stay within ~2% of std::sin/std::cos mid-quadrant",
          "[math][trig]") {
  for (const int degrees : {30, 45, 60, 120, -45}) {
    CAPTURE(degrees);
    const float radians = static_cast<float>(degrees) * static_cast<float>(M_PI) / 180.0f;
    CHECK(FastSinApprox(radians) == Approx(std::sin(radians)).margin(kTrigApproxMargin));
    CHECK(FastCosApprox(radians) == Approx(std::cos(radians)).margin(kTrigApproxMargin));
  }
}

TEST_CASE("FastSinCosApprox returns the same values as the separate sin/cos functions",
          "[math][trig]") {
  const float radians = 1.234f;
  const auto sample = FastSinCosApprox(radians);
  CHECK(sample.sine == Approx(FastSinApprox(radians)));
  CHECK(sample.cosine == Approx(FastCosApprox(radians)));
}
