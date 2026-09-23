#include "openwow/foundation/math/packed_affine4x3.h"
#include "openwow/foundation/math/packed_mat3x3.h"
#include "openwow/foundation/math/row_major_mat4x4.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

using Catch::Approx;
namespace mat4 = openwow::math::row_major_mat4x4;

namespace {

constexpr float kIdentity4x4[16] = {
    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
};

void CheckMatrixApprox(const float *actual, const float *expected, double margin = 0.0005) {
  for (int i = 0; i < 16; ++i) {
    CAPTURE(i);
    CHECK(actual[i] == Approx(expected[i]).margin(margin));
  }
}

} // namespace

TEST_CASE("SetIdentity produces the 4x4 identity matrix", "[math][mat4x4]") {
  float m[16] = {};
  mat4::SetIdentity(m);
  CheckMatrixApprox(m, kIdentity4x4);
}

TEST_CASE("EqualExact/NotEqualExact are exact componentwise comparisons", "[math][mat4x4]") {
  float a[16] = {};
  float b[16] = {};
  mat4::SetIdentity(a);
  mat4::SetIdentity(b);
  CHECK(mat4::EqualExact(a, b));
  CHECK_FALSE(mat4::NotEqualExact(a, b));

  b[5] = 2.0f;
  CHECK_FALSE(mat4::EqualExact(a, b));
  CHECK(mat4::NotEqualExact(a, b));
}

TEST_CASE(
    "EqualExact between two matrices both containing NaN in the same slot is false (IEEE 754)",
    "[math][mat4x4]") {
  float a[16] = {};
  float b[16] = {};
  mat4::SetIdentity(a);
  mat4::SetIdentity(b);
  a[0] = std::numeric_limits<float>::quiet_NaN();
  b[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK_FALSE(mat4::EqualExact(a, b));
}

TEST_CASE("Copy4x4 copies all 16 elements", "[math][mat4x4]") {
  float src[16];
  for (int i = 0; i < 16; ++i)
    src[i] = static_cast<float>(i);
  float dst[16] = {};
  CHECK(mat4::Copy4x4(dst, src) == dst);
  CheckMatrixApprox(dst, src);
}

TEST_CASE("BuildRowMajorAffine4x4FromAxesAndPosition assembles the basis and translation correctly",
          "[math][mat4x4]") {
  const float axis0[3] = {1.0f, 0.0f, 0.0f};
  const float axis1[3] = {0.0f, 1.0f, 0.0f};
  const float axis2[3] = {0.0f, 0.0f, 1.0f};
  const float position[3] = {10.0f, 20.0f, 30.0f};

  float m[16] = {};
  mat4::BuildRowMajorAffine4x4FromAxesAndPosition(m, axis0, axis1, axis2, position);

  // Axis-aligned basis + translation should reduce to the identity's
  // rotation part with the given translation in the last row.
  const float expected[16] = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  1.0f,  0.0f,  0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 20.0f, 30.0f, 1.0f,
  };
  CheckMatrixApprox(m, expected);
}

TEST_CASE("TransformPointByRowMajorAffine4x4Unbuffered: identity matrix leaves the point unchanged",
          "[math][mat4x4]") {
  const float point[3] = {1.0f, 2.0f, 3.0f};
  float out[3] = {};
  mat4::TransformPointByRowMajorAffine4x4Unbuffered(out, point, kIdentity4x4);
  CHECK(out[0] == Approx(1.0f));
  CHECK(out[1] == Approx(2.0f));
  CHECK(out[2] == Approx(3.0f));
}

TEST_CASE("TransformPointByRowMajorAffine4x4Unbuffered: a translation-only matrix shifts the point",
          "[math][mat4x4]") {
  float m[16] = {};
  mat4::SetIdentity(m);
  m[12] = 10.0f;
  m[13] = 20.0f;
  m[14] = 30.0f;

  const float point[3] = {1.0f, 2.0f, 3.0f};
  float out[3] = {};
  mat4::TransformPointByRowMajorAffine4x4Unbuffered(out, point, m);
  CHECK(out[0] == Approx(11.0f));
  CHECK(out[1] == Approx(22.0f));
  CHECK(out[2] == Approx(33.0f));
}

TEST_CASE("TransformPointByRowMajorAffine4x4 agrees with the Unbuffered variant "
          "(both read translation from the last row)",
          "[math][mat4x4]") {
  float rotation[9] = {};
  openwow::math::packed_mat3x3::BuildZRotationMatrix(rotation, 0.6f);
  float m[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedMat3x3(m, rotation);
  m[12] = 5.0f;
  m[13] = -3.0f;
  m[14] = 7.0f;

  const float point[3] = {1.0f, 2.0f, 3.0f};
  float unbuffered[3] = {};
  float buffered[3] = {};
  mat4::TransformPointByRowMajorAffine4x4Unbuffered(unbuffered, point, m);
  mat4::TransformPointByRowMajorAffine4x4(buffered, point, m);

  CHECK(buffered[0] == Approx(unbuffered[0]));
  CHECK(buffered[1] == Approx(unbuffered[1]));
  CHECK(buffered[2] == Approx(unbuffered[2]));
}

TEST_CASE("TransformPointByRowMajorAffine4x4LastColumn reads translation from the LAST COLUMN, "
          "not the last row",
          "[math][mat4x4]") {
  // A matrix that is the identity rotation with translation packed into
  // indices [3], [7], [11] (the last column of each row) instead of
  // [12]/[13]/[14] (the last row) -- the convention this specific
  // function's callers are expected to use, per its own code (it reads
  // matrix4x4[3]/[7]/[11] as the translation terms).
  float m[16] = {
      1.0f, 0.0f, 0.0f, 10.0f, 0.0f, 1.0f, 0.0f, 20.0f,
      0.0f, 0.0f, 1.0f, 30.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  const float point[3] = {1.0f, 2.0f, 3.0f};
  float out[3] = {};
  mat4::TransformPointByRowMajorAffine4x4LastColumn(out, m, point);
  CHECK(out[0] == Approx(11.0f));
  CHECK(out[1] == Approx(22.0f));
  CHECK(out[2] == Approx(33.0f));
}

TEST_CASE("TransformVec3ByUpper3x3 transforms a direction without applying translation",
          "[math][mat4x4]") {
  float m[16] = {};
  mat4::SetIdentity(m);
  m[12] = 100.0f; // translation that must NOT affect a direction vector
  m[13] = 100.0f;
  m[14] = 100.0f;

  const float direction[3] = {1.0f, 2.0f, 3.0f};
  float out[3] = {};
  mat4::TransformVec3ByUpper3x3(out, direction, m);
  CHECK(out[0] == Approx(1.0f));
  CHECK(out[1] == Approx(2.0f));
  CHECK(out[2] == Approx(3.0f));
}

TEST_CASE("Multiply4x4: identity is a two-sided multiplicative identity", "[math][mat4x4]") {
  float rotation[9] = {};
  openwow::math::packed_mat3x3::BuildZRotationMatrix(rotation, 0.4f);
  float m[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedMat3x3(m, rotation);
  m[12] = 1.0f;
  m[13] = 2.0f;
  m[14] = 3.0f;

  float left_identity[16] = {};
  mat4::Multiply4x4(left_identity, kIdentity4x4, m);
  CheckMatrixApprox(left_identity, m);

  float right_identity[16] = {};
  mat4::Multiply4x4(right_identity, m, kIdentity4x4);
  CheckMatrixApprox(right_identity, m);
}

TEST_CASE("BuildInverseRigidTransform4x4 inverts a rotation+translation matrix (M * M^-1 == I)",
          "[math][mat4x4]") {
  float rotation[9] = {};
  openwow::math::packed_mat3x3::BuildZRotationMatrix(rotation, 0.9f);
  float m[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedMat3x3(m, rotation);
  m[12] = 5.0f;
  m[13] = -2.0f;
  m[14] = 10.0f;

  float inverse[16] = {};
  mat4::BuildInverseRigidTransform4x4(inverse, m);

  float product[16] = {};
  mat4::Multiply4x4(product, m, inverse);
  CheckMatrixApprox(product, kIdentity4x4, 0.001);
}

TEST_CASE("BuildInverseRigidTransform4x4 writes every element of the 4x4 output, "
          "including the bottom row",
          "[math][mat4x4]") {
  // An earlier pass over this file's source (without running anything)
  // guessed that dst[11] was left unwritten, matching a real partial-write
  // footgun found elsewhere in this codebase's math helpers. Actually
  // building and running this test against the real implementation shows
  // that guess was wrong: dst[11] is explicitly set to 0.0f, and all 16
  // elements are written. Pinned here as a correction, and as a reminder
  // that guesses about behavior in this codebase need to be verified by
  // compiling and running, not just reading the source.
  float rotation[9] = {};
  openwow::math::packed_mat3x3::BuildZRotationMatrix(rotation, 0.3f);
  float m[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedMat3x3(m, rotation);

  float dst[16];
  for (float &value : dst)
    value = 12345.0f; // sentinel
  mat4::BuildInverseRigidTransform4x4(dst, m);
  for (int i = 0; i < 16; ++i) {
    CAPTURE(i);
    CHECK(dst[i] != 12345.0f);
  }
}

TEST_CASE("BuildRowMajorAffine4x4FromPackedMat3x3 / CopyRowMajorBasis3x3FromAffine4x4 round-trip",
          "[math][mat4x4]") {
  const float original3x3[9] = {
      1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,
  };
  float m4x4[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedMat3x3(m4x4, original3x3);
  CHECK(m4x4[3] == Approx(0.0f));
  CHECK(m4x4[15] == Approx(1.0f));

  float extracted3x3[9] = {};
  mat4::CopyRowMajorBasis3x3FromAffine4x4(extracted3x3, m4x4);
  for (int i = 0; i < 9; ++i) {
    CAPTURE(i);
    CHECK(extracted3x3[i] == Approx(original3x3[i]));
  }
}

TEST_CASE("BuildRowMajorAffine4x4FromPackedAffine4x3 embeds rotation and translation",
          "[math][mat4x4]") {
  const float affine4x3[12] = {
      1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 10.0f, 20.0f, 30.0f,
  };
  float m4x4[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedAffine4x3(m4x4, affine4x3);
  CHECK(m4x4[12] == Approx(10.0f));
  CHECK(m4x4[13] == Approx(20.0f));
  CHECK(m4x4[14] == Approx(30.0f));
  CHECK(m4x4[15] == Approx(1.0f));
}

TEST_CASE("TransformAABBByRowMajorAffine4x4 with a translation-only matrix just shifts min/max",
          "[math][mat4x4]") {
  float m[16] = {};
  mat4::SetIdentity(m);
  m[12] = 5.0f;
  m[13] = -5.0f;
  m[14] = 2.0f;

  const float bounds[6] = {0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f};
  float out_bounds[6] = {};
  mat4::TransformAABBByRowMajorAffine4x4(out_bounds, bounds, m);

  CHECK(out_bounds[0] == Approx(5.0f));
  CHECK(out_bounds[1] == Approx(-5.0f));
  CHECK(out_bounds[2] == Approx(2.0f));
  CHECK(out_bounds[3] == Approx(15.0f));
  CHECK(out_bounds[4] == Approx(5.0f));
  CHECK(out_bounds[5] == Approx(12.0f));
}
