#include "openwow/foundation/math/packed_affine4x3.h"
#include "openwow/foundation/math/packed_mat3x3.h"
#include "openwow/foundation/math/row_major_mat4x4.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
namespace affine4x3 = openwow::math::packed_affine4x3;
namespace mat3 = openwow::math::packed_mat3x3;
namespace mat4 = openwow::math::row_major_mat4x4;

namespace {

// affine4x3 layout: [0..8] is a row-major 3x3 rotation/scale (same
// convention as packed_mat3x3), [9..11] is the translation.
constexpr float kIdentityAffine4x3[12] = {
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
};

void CheckAffineApprox(const float *actual, const float *expected, double margin = 0.0005) {
  for (int i = 0; i < 12; ++i) {
    CAPTURE(i);
    CHECK(actual[i] == Approx(expected[i]).margin(margin));
  }
}

} // namespace

TEST_CASE("packed_affine4x3::Multiply: the identity affine transform is a two-sided identity",
          "[math][affine4x3]") {
  float rotation[9] = {};
  mat3::BuildZRotationMatrix(rotation, 0.5f);
  const float m[12] = {
      rotation[0], rotation[1], rotation[2], rotation[3], rotation[4], rotation[5],
      rotation[6], rotation[7], rotation[8], 10.0f,       -5.0f,       2.0f,
  };

  float right_identity[12] = {};
  affine4x3::Multiply(right_identity, m, kIdentityAffine4x3);
  CheckAffineApprox(right_identity, m);

  float left_identity[12] = {};
  affine4x3::Multiply(left_identity, kIdentityAffine4x3, m);
  CheckAffineApprox(left_identity, m);
}

TEST_CASE("packed_affine4x3::Multiply agrees with composing the equivalent 4x4 affine matrices",
          "[math][affine4x3]") {
  // Cross-checks two independently-implemented composition routines
  // against each other rather than hand-deriving the general 12-term
  // product formula: build the same two transforms as packed_affine4x3
  // and as row_major_mat4x4, compose each way, and confirm they describe
  // the same resulting transform.
  float left_rotation[9] = {};
  mat3::BuildZRotationMatrix(left_rotation, 0.4f);
  const float left_affine[12] = {
      left_rotation[0],
      left_rotation[1],
      left_rotation[2],
      left_rotation[3],
      left_rotation[4],
      left_rotation[5],
      left_rotation[6],
      left_rotation[7],
      left_rotation[8],
      3.0f,
      1.0f,
      -2.0f,
  };

  float right_rotation[9] = {};
  mat3::BuildZRotationMatrix(right_rotation, 0.9f);
  const float right_affine[12] = {
      right_rotation[0],
      right_rotation[1],
      right_rotation[2],
      right_rotation[3],
      right_rotation[4],
      right_rotation[5],
      right_rotation[6],
      right_rotation[7],
      right_rotation[8],
      -4.0f,
      5.0f,
      0.5f,
  };

  float product_affine[12] = {};
  affine4x3::Multiply(product_affine, left_affine, right_affine);
  float product_affine_as_4x4[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedAffine4x3(product_affine_as_4x4, product_affine);

  float left_4x4[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedAffine4x3(left_4x4, left_affine);
  float right_4x4[16] = {};
  mat4::BuildRowMajorAffine4x4FromPackedAffine4x3(right_4x4, right_affine);
  float expected_product_4x4[16] = {};
  mat4::Multiply4x4(expected_product_4x4, left_4x4, right_4x4);

  for (int i = 0; i < 16; ++i) {
    CAPTURE(i);
    CHECK(product_affine_as_4x4[i] == Approx(expected_product_4x4[i]).margin(0.001));
  }
}
