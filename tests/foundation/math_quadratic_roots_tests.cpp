#include "openwow/foundation/math/quadratic_roots.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using openwow::math::quadratic_roots::OrderedRoots;
using openwow::math::quadratic_roots::SolveOrderedStable;

TEST_CASE("SolveOrderedStable finds and orders the two real roots", "[math][quadratic]") {
  SECTION("x^2 - 5x + 6 = 0 -> roots 2, 3") {
    OrderedRoots roots;
    REQUIRE(SolveOrderedStable(1.0f, -5.0f, 6.0f, &roots));
    CHECK(roots.low == Approx(2.0f));
    CHECK(roots.high == Approx(3.0f));
  }

  SECTION("x^2 - 4 = 0 -> roots -2, 2") {
    OrderedRoots roots;
    REQUIRE(SolveOrderedStable(1.0f, 0.0f, -4.0f, &roots));
    CHECK(roots.low == Approx(-2.0f));
    CHECK(roots.high == Approx(2.0f));
  }

  SECTION("a negative leading coefficient still orders low <= high") {
    // -x^2 + 5x - 6 = 0 has the same roots as x^2 - 5x + 6 = 0.
    OrderedRoots roots;
    REQUIRE(SolveOrderedStable(-1.0f, 5.0f, -6.0f, &roots));
    CHECK(roots.low == Approx(2.0f));
    CHECK(roots.high == Approx(3.0f));
  }
}

TEST_CASE("SolveOrderedStable returns false for an exactly-zero discriminant (repeated root)",
          "[math][quadratic]") {
  // FLAGGED FOR FUTURE WORK: callers that expect "no roots" to mean
  // false and "a repeated root" to mean true-with-low==high will be
  // surprised -- a repeated root (discriminant == 0) reports false here,
  // identically to a genuinely complex (discriminant < 0) case. This is
  // pinned as documented current behavior; whether it should instead
  // report true with low==high depends on what callers actually need,
  // which is out of scope to change here.
  OrderedRoots roots{99.0f, 99.0f};
  CHECK_FALSE(SolveOrderedStable(1.0f, -2.0f, 1.0f, &roots)); // (x-1)^2 == 0
  // out_roots must be left untouched on failure.
  CHECK(roots.low == 99.0f);
  CHECK(roots.high == 99.0f);
}

TEST_CASE("SolveOrderedStable returns false for a negative discriminant (no real roots)",
          "[math][quadratic]") {
  CHECK_FALSE(SolveOrderedStable(1.0f, 0.0f, 1.0f, nullptr)); // x^2 + 1 == 0
}

TEST_CASE("SolveOrderedStable tolerates a null out_roots pointer on success", "[math][quadratic]") {
  CHECK(SolveOrderedStable(1.0f, -5.0f, 6.0f, nullptr));
}
