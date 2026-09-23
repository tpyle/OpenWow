#include "openwow/foundation/math/packed_quaternion64.h"
#include "openwow/foundation/math/quaternion_xyzw.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
namespace pq64 = openwow::math::packed_quaternion64;

TEST_CASE("packed_quaternion64::SignExtend21Bit sign-extends a 21-bit two's complement value",
          "[math][quaternion64]") {
  CHECK(pq64::SignExtend21Bit(0) == 0);
  CHECK(pq64::SignExtend21Bit(1) == 1);
  CHECK(pq64::SignExtend21Bit(0x1FFFFF) == -1);       // all 21 bits set == -1
  CHECK(pq64::SignExtend21Bit(0x100000) == -1048576); // sign bit alone == the minimum value
  CHECK(pq64::SignExtend21Bit(0xFFFFF) == 1048575);   // largest positive 21-bit value
}

TEST_CASE("packed_quaternion64::Compress/Decompress round-trips a unit quaternion within "
          "quantization error",
          "[math][quaternion64]") {
  const float axis[3] = {0.0f, 0.0f, 1.0f};
  // Angle chosen so w stays clearly positive: Compress() canonicalizes to
  // w >= 0 by folding w's sign into x/y/z, and Decompress() always
  // reconstructs a non-negative w -- an input with w < 0 would not
  // round-trip component-for-component (see the flagged test below).
  const auto q = openwow::math::quaternion_xyzw::FromAxisAngle(axis, 0.5f);
  REQUIRE(q.w >= 0.0f);

  const std::int64_t packed = pq64::Compress(q.x, q.y, q.z, q.w);

  float out_x = 0.0f, out_y = 0.0f, out_z = 0.0f, out_w = 0.0f;
  pq64::Decompress(packed, out_x, out_y, out_z, out_w);

  const double kToleranceX = 2.0 * pq64::kInvScaleX;
  const double kToleranceYZ = 2.0 * pq64::kInvScaleYZ;
  CHECK(out_x == Approx(q.x).margin(kToleranceX));
  CHECK(out_y == Approx(q.y).margin(kToleranceYZ));
  CHECK(out_z == Approx(q.z).margin(kToleranceYZ));
  CHECK(out_w == Approx(q.w).margin(kToleranceYZ * 4)); // w is derived, error compounds
}

TEST_CASE("packed_quaternion64::Compress array-argument overload matches the four-float overload",
          "[math][quaternion64]") {
  const float xyzw[4] = {0.1f, 0.2f, 0.3f, 0.9f};
  CHECK(pq64::Compress(xyzw) == pq64::Compress(xyzw[0], xyzw[1], xyzw[2], xyzw[3]));
}

TEST_CASE("packed_quaternion64::Decompress reconstructs a non-negative w even when the "
          "original quaternion had w < 0",
          "[math][quaternion64]") {
  // FLAGGED FOR FUTURE WORK (documented, not a bug): quaternions q and -q
  // represent the same rotation, and Compress() relies on this to fold an
  // arbitrary w's sign into x/y/z so only 3 components need to be stored.
  // A caller that round-trips a quaternion through Compress/Decompress and
  // expects the exact original w back (rather than an equivalent rotation)
  // will be surprised when w's sign flips.
  const float x = 0.0f, y = 0.0f, z = 0.3f, w = -std::sqrt(1.0f - 0.09f);
  const std::int64_t packed = pq64::Compress(x, y, z, w);

  float out_x = 0.0f, out_y = 0.0f, out_z = 0.0f, out_w = 0.0f;
  pq64::Decompress(packed, out_x, out_y, out_z, out_w);
  CHECK(out_w >= 0.0f); // always non-negative, regardless of the original sign
}
