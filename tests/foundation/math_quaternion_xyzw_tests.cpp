#include "openwow/foundation/math/quaternion_xyzw.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
using openwow::math::quaternion_xyzw::FromAxisAngle;
using openwow::math::quaternion_xyzw::LerpQuaternion;
using openwow::math::quaternion_xyzw::Multiply;
using openwow::math::quaternion_xyzw::Quaternion;
using openwow::math::quaternion_xyzw::QuatSlerp;

TEST_CASE("Multiply: the identity quaternion is a two-sided multiplicative identity",
          "[math][quaternion]") {
  const Quaternion identity{0.0f, 0.0f, 0.0f, 1.0f};
  const Quaternion q{0.2f, -0.3f, 0.4f, 0.8f};

  const Quaternion right = Multiply(q, identity);
  CHECK(right.x == Approx(q.x));
  CHECK(right.y == Approx(q.y));
  CHECK(right.z == Approx(q.z));
  CHECK(right.w == Approx(q.w));

  const Quaternion left = Multiply(identity, q);
  CHECK(left.x == Approx(q.x));
  CHECK(left.y == Approx(q.y));
  CHECK(left.z == Approx(q.z));
  CHECK(left.w == Approx(q.w));
}

TEST_CASE("Multiply: composing two 90-degree Z rotations yields a 180-degree Z rotation",
          "[math][quaternion]") {
  const float z_axis[3] = {0.0f, 0.0f, 1.0f};
  const Quaternion quarter_turn = FromAxisAngle(z_axis, static_cast<float>(M_PI_2));

  const Quaternion half_turn = Multiply(quarter_turn, quarter_turn);
  CHECK(half_turn.x == Approx(0.0f).margin(0.0005));
  CHECK(half_turn.y == Approx(0.0f).margin(0.0005));
  CHECK(half_turn.z == Approx(1.0f).margin(0.0005));
  CHECK(half_turn.w == Approx(0.0f).margin(0.0005));
}

TEST_CASE("FromAxisAngle: the struct-returning and out-parameter overloads agree",
          "[math][quaternion]") {
  const float axis[3] = {0.0f, 1.0f, 0.0f};
  const float angle = 0.85f;

  const Quaternion from_struct = FromAxisAngle(axis, angle);

  float out[4] = {};
  FromAxisAngle(out, angle, axis);

  CHECK(out[0] == Approx(from_struct.x));
  CHECK(out[1] == Approx(from_struct.y));
  CHECK(out[2] == Approx(from_struct.z));
  CHECK(out[3] == Approx(from_struct.w));
}

TEST_CASE("FromAxisAngle: angle 0 yields the identity quaternion", "[math][quaternion]") {
  const float axis[3] = {1.0f, 0.0f, 0.0f};
  const Quaternion q = FromAxisAngle(axis, 0.0f);
  CHECK(q.x == Approx(0.0f));
  CHECK(q.y == Approx(0.0f));
  CHECK(q.z == Approx(0.0f));
  CHECK(q.w == Approx(1.0f));
}

TEST_CASE("FastNormalizeInPlace: accurate for already-near-unit-length input",
          "[math][quaternion]") {
  // This is a fast polynomial approximation of 1/sqrt(lengthSq), refined
  // with 1-3 Newton iterations depending on how far lengthSq starts from 1.
  // It is only accurate near unit length (the range quaternion lerp/slerp
  // results actually land in) -- see the flagged test below for how badly
  // it degrades outside that range.
  Quaternion q{0.5f, 0.5f, 0.5f, 0.5f}; // lengthSq == 1.0 already
  openwow::math::quaternion_xyzw::FastNormalizeInPlace(q);
  const float length_sq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  CHECK(length_sq == Approx(1.0f).margin(0.001));
}

TEST_CASE("FastNormalizeInPlace: FLAGGED FOR FUTURE WORK -- degrades badly far from unit length",
          "[math][quaternion]") {
  // Verified numerically: for lengthSq == 2.0 (e.g. an un-normalized
  // (1,0,0,1)), the "fast" normalize leaves the result at length ~0.66,
  // nowhere near 1. Any caller that might feed this a non-near-unit
  // quaternion (as opposed to the output of a lerp between two unit
  // quaternions, which is what it's designed for) will get silently wrong
  // results with no error signal. Pinned here as documented current
  // behavior, not fixed.
  Quaternion q{1.0f, 0.0f, 0.0f, 1.0f}; // lengthSq == 2.0
  openwow::math::quaternion_xyzw::FastNormalizeInPlace(q);
  const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  CHECK(length < 0.7f); // should be ~1.0 for a correct normalize
  CHECK(length > 0.6f);
}

TEST_CASE("LerpQuaternion: the struct and float* overloads agree and produce a normalized result",
          "[math][quaternion]") {
  const Quaternion a{0.0f, 0.0f, 0.0f, 1.0f};
  const Quaternion b{0.0f, 0.0f, 1.0f, 0.0f};

  const Quaternion from_struct = LerpQuaternion(a, b, 0.5f);

  float out[4] = {};
  const float a4[4] = {a.x, a.y, a.z, a.w};
  const float b4[4] = {b.x, b.y, b.z, b.w};
  LerpQuaternion(out, 0.5f, a4, b4);

  CHECK(out[0] == Approx(from_struct.x));
  CHECK(out[1] == Approx(from_struct.y));
  CHECK(out[2] == Approx(from_struct.z));
  CHECK(out[3] == Approx(from_struct.w));

  const float length_sq = from_struct.x * from_struct.x + from_struct.y * from_struct.y +
                          from_struct.z * from_struct.z + from_struct.w * from_struct.w;
  CHECK(length_sq == Approx(1.0f).margin(0.001));
}

TEST_CASE("QuatSlerp: t=0 returns the start quaternion and t=1 returns the end quaternion",
          "[math][quaternion]") {
  // Keep a/b close together (small angle apart) so their dot product is
  // positive and QuatSlerp does not need to flip sign to take the short
  // path -- that sign flip is expected/correct SLERP behavior (q and -q
  // represent the same rotation) but would make a direct component
  // comparison at t=1 misleading here.
  const float axis[3] = {0.0f, 0.0f, 1.0f};
  const Quaternion a = FromAxisAngle(axis, 0.1f);
  const Quaternion b = FromAxisAngle(axis, 0.3f);
  const float a4[4] = {a.x, a.y, a.z, a.w};
  const float b4[4] = {b.x, b.y, b.z, b.w};

  float at_start[4] = {};
  QuatSlerp(at_start, 0.0f, a4, b4);
  CHECK(at_start[0] == Approx(a.x).margin(0.0005));
  CHECK(at_start[1] == Approx(a.y).margin(0.0005));
  CHECK(at_start[2] == Approx(a.z).margin(0.0005));
  CHECK(at_start[3] == Approx(a.w).margin(0.0005));

  float at_end[4] = {};
  QuatSlerp(at_end, 1.0f, a4, b4);
  CHECK(at_end[0] == Approx(b.x).margin(0.0005));
  CHECK(at_end[1] == Approx(b.y).margin(0.0005));
  CHECK(at_end[2] == Approx(b.z).margin(0.0005));
  CHECK(at_end[3] == Approx(b.w).margin(0.0005));
}

TEST_CASE("QuatSlerp: near-identical inputs take the small-angle early-return path unchanged",
          "[math][quaternion]") {
  // The early-return triggers on sinOmega = sqrt(|1 - dot^2|) being near
  // zero, which requires dot(a, a) == 1 -- i.e. a genuine *unit*
  // quaternion. An earlier version of this test used a non-normalized
  // (0.1, 0.2, 0.3, 0.9) (|.|^2 == 0.95) and failed: with dot == 0.95 the
  // early-return path is not taken at all, and QuatSlerp instead does a
  // real (non-degenerate) interpolation that scales the identical inputs
  // by a factor other than 1. Fixed by using an actual unit quaternion.
  const float axis[3] = {0.267261242f, 0.534522484f, 0.801783726f}; // normalize(1,2,3)
  Quaternion q = FromAxisAngle(axis, 0.4f);
  const float a4[4] = {q.x, q.y, q.z, q.w};
  const float b4[4] = {q.x, q.y, q.z, q.w}; // identical -> sinOmega ~ 0

  float out[4] = {};
  QuatSlerp(out, 0.5f, a4, b4);
  CHECK(out[0] == Approx(a4[0]));
  CHECK(out[1] == Approx(a4[1]));
  CHECK(out[2] == Approx(a4[2]));
  CHECK(out[3] == Approx(a4[3]));
}
