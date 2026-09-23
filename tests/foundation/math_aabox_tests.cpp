#include "openwow/foundation/math/aabox_compute.h"
#include "openwow/foundation/math/aabox_contains_point.h"
#include "openwow/foundation/math/aabox_init_from_position.h"
#include "openwow/foundation/math/aabox_isvalid.h"
#include "openwow/foundation/math/aabox_translate.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using Catch::Approx;
namespace aabox = openwow::math::aabox;

TEST_CASE("aabox::ComputeAABB finds the min/max extent of a point cloud", "[math][aabox]") {
  const float points[] = {
      1.0f, 5.0f, -2.0f, -3.0f, 2.0f, 4.0f, 0.0f, -1.0f, 0.0f,
  };
  float box[6] = {};
  CHECK(aabox::ComputeAABB(box, points, 3) == box);
  CHECK(box[0] == Approx(-3.0f)); // min x
  CHECK(box[1] == Approx(-1.0f)); // min y
  CHECK(box[2] == Approx(-2.0f)); // min z
  CHECK(box[3] == Approx(1.0f));  // max x
  CHECK(box[4] == Approx(5.0f));  // max y
  CHECK(box[5] == Approx(4.0f));  // max z
}

TEST_CASE("aabox::ComputeAABB with a single point produces a degenerate box", "[math][aabox]") {
  const float points[] = {1.0f, 2.0f, 3.0f};
  float box[6] = {};
  aabox::ComputeAABB(box, points, 1);
  CHECK(box[0] == Approx(1.0f));
  CHECK(box[3] == Approx(1.0f));
  CHECK(box[1] == Approx(2.0f));
  CHECK(box[4] == Approx(2.0f));
}

TEST_CASE("aabox::ComputeAABB with zero points zeroes the output rather than leaving it untouched",
          "[math][aabox]") {
  float box[6] = {99.0f, 99.0f, 99.0f, 99.0f, 99.0f, 99.0f};
  aabox::ComputeAABB(box, nullptr, 0);
  for (const float component : box) {
    CHECK(component == 0.0f);
  }
}

TEST_CASE("aabox::InitFromPosition creates a degenerate (zero-volume) box at a point",
          "[math][aabox]") {
  const float position[3] = {1.0f, 2.0f, 3.0f};
  float box[6] = {};
  CHECK(aabox::InitFromPosition(box, position) == box);
  CHECK(box[0] == Approx(1.0f));
  CHECK(box[3] == Approx(1.0f));
  CHECK(box[1] == Approx(2.0f));
  CHECK(box[4] == Approx(2.0f));
  CHECK(box[2] == Approx(3.0f));
  CHECK(box[5] == Approx(3.0f));
}

TEST_CASE("aabox::IsValid rejects zero-volume and inverted boxes", "[math][aabox]") {
  SECTION("a real box with positive extent on all axes is valid") {
    const float box[6] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    CHECK(aabox::IsValid(box));
  }

  SECTION("a zero-volume box (min == max) is not valid -- strict comparison") {
    const float box[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    CHECK_FALSE(aabox::IsValid(box));
  }

  SECTION("an inverted box (min > max on one axis) is not valid") {
    const float box[6] = {5.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    CHECK_FALSE(aabox::IsValid(box));
  }
}

TEST_CASE("aabox::ContainsPoint uses strict inequality (boundary points do not count)",
          "[math][aabox]") {
  const float box[6] = {0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f};

  SECTION("a point strictly inside is contained") {
    const float point[3] = {5.0f, 5.0f, 5.0f};
    CHECK(aabox::ContainsPoint(box, point));
  }

  SECTION("a point exactly on the min boundary is NOT contained") {
    const float point[3] = {0.0f, 5.0f, 5.0f};
    CHECK_FALSE(aabox::ContainsPoint(box, point));
  }

  SECTION("a point exactly on the max boundary is NOT contained") {
    const float point[3] = {5.0f, 5.0f, 10.0f};
    CHECK_FALSE(aabox::ContainsPoint(box, point));
  }

  SECTION("a point fully outside is not contained") {
    const float point[3] = {20.0f, 5.0f, 5.0f};
    CHECK_FALSE(aabox::ContainsPoint(box, point));
  }
}

TEST_CASE("aabox::Translate shifts both min and max by the offset", "[math][aabox]") {
  float box[6] = {0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f};
  const float offset[3] = {1.0f, -2.0f, 3.0f};
  aabox::Translate(box, offset);
  CHECK(box[0] == Approx(1.0f));
  CHECK(box[1] == Approx(-2.0f));
  CHECK(box[2] == Approx(3.0f));
  CHECK(box[3] == Approx(11.0f));
  CHECK(box[4] == Approx(8.0f));
  CHECK(box[5] == Approx(13.0f));
}

TEST_CASE("aabox::Translate's return value is the offset pointer, not the translated box",
          "[math][aabox]") {
  // FLAGGED FOR FUTURE WORK: every sibling aabox_*.h function that mutates
  // and returns a pointer returns the *mutated* buffer (box/out), letting
  // callers chain e.g. `Foo(box, ...)`. Translate is the one exception --
  // it mutates `box` correctly but returns `offset` unchanged. This is
  // almost certainly an oversight rather than an intentional design choice,
  // but changing it is a (small) API/ABI-shape change for any caller that
  // relies on the current return value, so it's pinned here as documented
  // current behavior rather than "fixed" as part of adding tests.
  float box[6] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
  const float offset[3] = {5.0f, 5.0f, 5.0f};
  const float *const result = aabox::Translate(box, offset);
  CHECK(result == offset);
  CHECK(result != box);
}
