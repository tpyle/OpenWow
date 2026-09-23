#include "openwow/foundation/math/client_rounding.h"
#include "openwow/foundation/math/copysign_float.h"
#include "openwow/foundation/math/get_dominant_axis.h"
#include "openwow/foundation/math/isnan_double.h"
#include "openwow/foundation/math/projection_aspect.h"

#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

using Catch::Approx;
using namespace openwow::math;

// --- client_rounding.h --------------------------------------------------

TEST_CASE("TruncateFloatToIntTowardZero truncates toward zero for both signs", "[math][rounding]") {
  CHECK(TruncateFloatToIntTowardZero(3.9) == 3);
  CHECK(TruncateFloatToIntTowardZero(-3.9) == -3);
  CHECK(TruncateFloatToIntTowardZero(0.0) == 0);
}

TEST_CASE("RoundFloatHalfUpToInt rounds .5 toward positive infinity", "[math][rounding]") {
  CHECK(RoundFloatHalfUpToInt(2.5f) == 3);
  CHECK(RoundFloatHalfUpToInt(2.4f) == 2);
  // -2.5 + 0.5 = -2.0, truncated toward zero -> -2 (i.e. rounds "up" even
  // for negative halves, unlike RoundFloatHalfAwayFromZero below).
  CHECK(RoundFloatHalfUpToInt(-2.5f) == -2);
}

TEST_CASE("RoundFloatHalfAwayFromZero rounds .5 away from zero regardless of sign",
          "[math][rounding]") {
  CHECK(RoundFloatHalfAwayFromZero(2.5f) == 3);
  CHECK(RoundFloatHalfAwayFromZero(-2.5f) == -3);
  CHECK(RoundFloatHalfAwayFromZero(0.4f) == 0);
  CHECK(RoundFloatHalfAwayFromZero(-0.4f) == 0);
}

TEST_CASE("RoundFloatHalfUpToInt and RoundFloatHalfAwayFromZero disagree on negative halves",
          "[math][rounding]") {
  // This pins the exact contrast the two "round" functions have for
  // negative .5 boundaries -- easy to accidentally treat as equivalent
  // when reading the code, so pinned explicitly here.
  CHECK(RoundFloatHalfUpToInt(-2.5f) == -2);
  CHECK(RoundFloatHalfAwayFromZero(-2.5f) == -3);
}

TEST_CASE("LegacyLineSpacingSnapToInt always adds the positive bias, even for negative input",
          "[math][rounding]") {
  CHECK(LegacyLineSpacingSnapToInt(0.0f) == 0);
  CHECK(LegacyLineSpacingSnapToInt(1.0f) == 1);
  // -1.5 + 0.99994999 = -0.50005..., truncated toward zero -> 0, not -1.
  CHECK(LegacyLineSpacingSnapToInt(-1.5f) == 0);
}

TEST_CASE("LegacyPixelSnapToInt only adds the positive bias for strictly-positive input",
          "[math][rounding]") {
  CHECK(LegacyPixelSnapToInt(0.0f) == 0);
  CHECK(LegacyPixelSnapToInt(1.0f) == 1);
  // No bias applied since -1.5 is not > 0: trunc(-1.5) == -1.
  CHECK(LegacyPixelSnapToInt(-1.5f) == -1);
}

TEST_CASE("LegacyLineSpacingSnapToInt and LegacyPixelSnapToInt disagree for negative input",
          "[math][rounding]") {
  // FLAGGED FOR FUTURE WORK: these two functions have near-identical names
  // and near-identical implementations (same bias constant), differing
  // only in whether the bias is applied unconditionally
  // (LegacyLineSpacingSnap) or only for value > 0 (LegacyPixelSnap). For
  // positive input they agree; for negative input they diverge, as pinned
  // here. Worth a doc comment at the call sites (or a shared helper) so a
  // future reader doesn't assume they're interchangeable.
  CHECK(LegacyLineSpacingSnapToInt(-1.5f) != LegacyPixelSnapToInt(-1.5f));
  CHECK(LegacyLineSpacingSnapToInt(1.5f) == LegacyPixelSnapToInt(1.5f));
}

TEST_CASE("LegacyLineSpacingSnapToFloat/LegacyPixelSnapToFloat mirror their int counterparts",
          "[math][rounding]") {
  CHECK(LegacyLineSpacingSnapToFloat(1.0f) ==
        Approx(static_cast<float>(LegacyLineSpacingSnapToInt(1.0f))));
  CHECK(LegacyPixelSnapToFloat(1.0f) == Approx(static_cast<float>(LegacyPixelSnapToInt(1.0f))));
}

TEST_CASE("FloorFloat matches std::floor", "[math][rounding]") {
  CHECK(FloorFloat(1.9f) == Approx(1.0));
  CHECK(FloorFloat(-1.1f) == Approx(-2.0));
  CHECK(FloorFloat(2.0f) == Approx(2.0));
}

// --- copysign_float.h -----------------------------------------------------

TEST_CASE("CopySignFloat matches std::copysign across a sign/magnitude matrix",
          "[math][copysign]") {
  const float magnitudes[] = {0.0f, 5.0f, -5.0f, std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()};
  const float signs[] = {1.0f, -1.0f, 0.0f, -0.0f};

  for (const float magnitude : magnitudes) {
    for (const float sign : signs) {
      CAPTURE(magnitude, sign);
      const float expected = std::copysign(magnitude, sign);
      const float actual = CopySignFloat(magnitude, sign);
      CHECK(std::signbit(actual) == std::signbit(expected));
      if (std::isnan(expected)) {
        CHECK(std::isnan(actual));
      } else {
        CHECK(std::fabs(actual) == Approx(std::fabs(expected)));
      }
    }
  }
}

TEST_CASE("CopySignFloat with a negative-zero sign source produces a negative result",
          "[math][copysign]") {
  const float result = CopySignFloat(5.0f, -0.0f);
  CHECK(result == Approx(-5.0f));
  CHECK(std::signbit(result));
}

// --- isnan_double.h ---------------------------------------------------

TEST_CASE("IsNaN returns 1 for NaN and 0 for any finite/infinite value", "[math][isnan]") {
  CHECK(IsNaN(std::numeric_limits<double>::quiet_NaN()) == 1);
  CHECK(IsNaN(0.0) == 0);
  CHECK(IsNaN(-1.5) == 0);
  CHECK(IsNaN(std::numeric_limits<double>::infinity()) == 0);
  CHECK(IsNaN(-std::numeric_limits<double>::infinity()) == 0);
}

// --- get_dominant_axis.h -----------------------------------------------

TEST_CASE("GetDominantAxis picks the axis with the largest-magnitude component",
          "[math][dominant_axis]") {
  SECTION("x clearly dominant") {
    const float v[3] = {5.0f, 1.0f, 2.0f};
    CHECK(GetDominantAxis(v) == 0);
  }
  SECTION("y clearly dominant") {
    const float v[3] = {1.0f, 5.0f, 2.0f};
    CHECK(GetDominantAxis(v) == 1);
  }
  SECTION("z clearly dominant") {
    const float v[3] = {1.0f, 2.0f, 5.0f};
    CHECK(GetDominantAxis(v) == 2);
  }
  SECTION("magnitude, not sign, decides") {
    const float v[3] = {-5.0f, 1.0f, 2.0f};
    CHECK(GetDominantAxis(v) == 0);
  }
}

TEST_CASE("GetDominantAxis tie-breaking is asymmetric: an x==y tie favors y, other ties favor z",
          "[math][dominant_axis]") {
  // The tie-breaking here is non-obvious from a skim: `abs_x <= abs_y`
  // treats an x==y tie the same as "y is bigger", so it falls into the
  // branch that can return 1 (y) -- and does, whenever y (tied with x or
  // not) still strictly beats z. Every *other* tie (y==z, x==z, or a full
  // 3-way tie) falls through to the final `return 2`. Pinned per-case
  // since it's easy to misjudge which axis "wins" a tie by inspection.
  CHECK(GetDominantAxis(std::array<float, 3>{1.0f, 1.0f, 1.0f}.data()) == 2); // 3-way tie
  CHECK(GetDominantAxis(std::array<float, 3>{1.0f, 2.0f, 2.0f}.data()) == 2); // y==z tie, x smaller
  CHECK(GetDominantAxis(std::array<float, 3>{2.0f, 2.0f, 1.0f}.data()) ==
        1); // x==y tie, both > z -> y wins
  CHECK(GetDominantAxis(std::array<float, 3>{2.0f, 1.0f, 2.0f}.data()) == 2); // x==z tie
}

// --- projection_aspect.h ------------------------------------------------

TEST_CASE("ComputeAspectPx returns width/height for positive dimensions", "[math][projection]") {
  CHECK(projection::ComputeAspectPx(1920, 1080) == Approx(1920.0f / 1080.0f));
  CHECK(projection::ComputeAspectPx(4, 2) == Approx(2.0f));
}

TEST_CASE("ComputeAspectPx falls back to 1.0 for non-positive dimensions", "[math][projection]") {
  CHECK(projection::ComputeAspectPx(0, 1080) == Approx(1.0f));
  CHECK(projection::ComputeAspectPx(1920, 0) == Approx(1.0f));
  CHECK(projection::ComputeAspectPx(-100, 1080) == Approx(1.0f));
  CHECK(projection::ComputeAspectPx(1920, -100) == Approx(1.0f));
}
