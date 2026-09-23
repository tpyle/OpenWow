#include "openwow/foundation/math/exp2_approx.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

using Catch::Approx;
using openwow::math::exp2_approx::Evaluate;
using openwow::math::exp2_approx::FromNaturalExponent;

TEST_CASE("exp2_approx::Evaluate matches 2^x at exact and near-exact powers of two",
          "[math][exp2]") {
  CHECK(Evaluate(0.0) == Approx(1.0).margin(1e-9));
  CHECK(Evaluate(1.0) == Approx(2.0).margin(1e-9));
  CHECK(Evaluate(-1.0) == Approx(0.5).margin(1e-9));
  CHECK(Evaluate(3.0) == Approx(8.0).margin(1e-9));
  CHECK(Evaluate(10.0) == Approx(1024.0).margin(1e-6));
  CHECK(Evaluate(0.5) == Approx(std::sqrt(2.0)).margin(1e-9));
  CHECK(Evaluate(-0.5) == Approx(1.0 / std::sqrt(2.0)).margin(1e-9));
}

TEST_CASE("exp2_approx::Evaluate saturates to 0 on underflow and +infinity on overflow",
          "[math][exp2]") {
  CHECK(Evaluate(-2000.0) == 0.0);
  CHECK(std::isinf(Evaluate(2000.0)));
  CHECK(Evaluate(2000.0) > 0.0); // +infinity, not -infinity
}

TEST_CASE("exp2_approx::Evaluate: FLAGGED FOR FUTURE WORK -- a NaN input silently returns "
          "+infinity instead of propagating NaN",
          "[math][exp2]") {
  // Verified numerically against the actual bit-level algorithm: the
  // underflow guard is `if (!(exponent > -1022.0)) return exponent ==
  // exponent ? 0.0 : kInfinity;` -- NaN fails `exponent == exponent`, so
  // it takes the *infinity* branch rather than a NaN-propagating one. Any
  // caller that feeds this a NaN (e.g. from an earlier 0/0 or inf-inf)
  // gets a silently "valid-looking" +infinity instead of a NaN they could
  // detect. Pinned here as documented current behavior, not fixed.
  const double result = Evaluate(std::numeric_limits<double>::quiet_NaN());
  CHECK(std::isinf(result));
  CHECK(result > 0.0);
}

TEST_CASE("exp2_approx::FromNaturalExponent approximates e^x", "[math][exp2]") {
  // Less precise than Evaluate() itself: this multiplies by a *float*
  // (single precision) log2(e) constant before delegating to Evaluate(),
  // so the achievable accuracy is bounded by float precision, not
  // Evaluate()'s own (much tighter) double-precision accuracy.
  CHECK(FromNaturalExponent(0.0) == Approx(1.0).margin(1e-6));
  CHECK(FromNaturalExponent(1.0) == Approx(M_E).margin(1e-5));
  CHECK(FromNaturalExponent(-1.0) == Approx(1.0 / M_E).margin(1e-5));
}
