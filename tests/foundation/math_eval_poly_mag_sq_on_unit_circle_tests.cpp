#include "openwow/foundation/math/eval_poly_mag_sq_on_unit_circle.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
using openwow::math::EvalPolyMagSqOnUnitCircle;
using openwow::math::kPolyCoeffCount;

// EvalPolyMagSqOnUnitCircle evaluates |1 + z * P(z)|^2 for z = e^(i*theta),
// where P(z) = sum_{k=0}^{9} coefficients[k] * z^k, via a Horner-style
// complex recurrence. Derived by hand-tracing the recurrence against that
// closed form (not guessed): for a single unit coefficient at index k
// (all others zero), P(z) = z^k, so the result reduces to
// |1 + z^(k+1)|^2 == 2 + 2*cos((k+1)*theta).

TEST_CASE("EvalPolyMagSqOnUnitCircle is 1 everywhere when all coefficients are zero",
          "[math][polycircle]") {
  float coefficients[kPolyCoeffCount] = {};
  for (const float theta : {0.0f, 0.5f, 1.5f, 3.14159f, -2.0f}) {
    CAPTURE(theta);
    CHECK(EvalPolyMagSqOnUnitCircle(coefficients, theta) == Approx(1.0).margin(1e-6));
  }
}

TEST_CASE("EvalPolyMagSqOnUnitCircle with a single unit coefficient matches "
          "2 + 2*cos((k+1)*theta)",
          "[math][polycircle]") {
  for (const int k : {0, 1, 4, 9}) {
    CAPTURE(k);
    float coefficients[kPolyCoeffCount] = {};
    coefficients[k] = 1.0f;
    for (const float theta : {0.0f, 0.7f, static_cast<float>(M_PI), 2.4f}) {
      CAPTURE(theta);
      const double expected = 2.0 + 2.0 * std::cos(static_cast<double>(k + 1) * theta);
      CHECK(EvalPolyMagSqOnUnitCircle(coefficients, theta) == Approx(expected).margin(0.001));
    }
  }
}
