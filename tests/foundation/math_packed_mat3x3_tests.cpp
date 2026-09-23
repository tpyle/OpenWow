#include "openwow/foundation/math/packed_mat3x3.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
namespace mat3 = openwow::math::packed_mat3x3;

namespace {

constexpr float kIdentity[9] = {
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
};

void CheckMatrixApprox(const float *actual, const float *expected, double margin = 0.0005) {
  for (int i = 0; i < 9; ++i) {
    CAPTURE(i);
    CHECK(actual[i] == Approx(expected[i]).margin(margin));
  }
}

} // namespace

TEST_CASE("packed_mat3x3::Determinant of the identity matrix is 1", "[math][mat3x3]") {
  CHECK(mat3::Determinant(kIdentity) == Approx(1.0));
}

TEST_CASE("packed_mat3x3::Determinant of a singular matrix is 0", "[math][mat3x3]") {
  // Second row is a multiple of the first -> linearly dependent -> singular.
  const float singular[9] = {
      1.0f, 2.0f, 3.0f, 2.0f, 4.0f, 6.0f, 0.0f, 0.0f, 1.0f,
  };
  CHECK(mat3::Determinant(singular) == Approx(0.0).margin(0.0001));
}

TEST_CASE("packed_mat3x3::SetRowMajor lays out arguments in row-major order", "[math][mat3x3]") {
  float m[9] = {};
  mat3::SetRowMajor(m, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  for (int i = 0; i < 9; ++i) {
    CHECK(m[i] == Approx(static_cast<float>(i + 1)));
  }
}

TEST_CASE("packed_mat3x3::ComputeAdjugate of the identity matrix is the identity matrix",
          "[math][mat3x3]") {
  float adjugate[9] = {};
  mat3::ComputeAdjugate(adjugate, kIdentity);
  CheckMatrixApprox(adjugate, kIdentity);
}

TEST_CASE("packed_mat3x3::InvertWithDeterminant inverts a real matrix (M * M^-1 == I)",
          "[math][mat3x3]") {
  // A simple invertible (diagonal scale) matrix.
  const float m[9] = {
      2.0f, 0.0f, 0.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f, 0.5f,
  };
  const double det = mat3::Determinant(m);
  REQUIRE(det != 0.0);

  float inverse[9] = {};
  mat3::InvertWithDeterminant(inverse, m, static_cast<float>(det));

  float product[9] = {};
  mat3::MultiplyRowMajor(product, m, inverse);
  CheckMatrixApprox(product, kIdentity);
}

TEST_CASE("packed_mat3x3::ScaleRows scales each row by the matching scalar", "[math][mat3x3]") {
  float m[9] = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
  };
  mat3::ScaleRows(m, 2.0f, 3.0f, 4.0f);
  const float expected[9] = {
      2.0f, 2.0f, 2.0f, 3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f,
  };
  CheckMatrixApprox(m, expected);
}

TEST_CASE("packed_mat3x3::ScaleRowsFromVec3 matches ScaleRows and returns the scale pointer",
          "[math][mat3x3]") {
  float m[9] = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
  };
  const float scale[3] = {2.0f, 3.0f, 4.0f};
  CHECK(mat3::ScaleRowsFromVec3(m, scale) == scale);
  const float expected[9] = {
      2.0f, 2.0f, 2.0f, 3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f,
  };
  CheckMatrixApprox(m, expected);
}

TEST_CASE("packed_mat3x3::MultiplyRowMajor: identity is a two-sided multiplicative identity",
          "[math][mat3x3]") {
  const float m[9] = {
      1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 10.0f, // not singular
  };

  float left_identity[9] = {};
  mat3::MultiplyRowMajor(left_identity, kIdentity, m);
  CheckMatrixApprox(left_identity, m);

  float right_identity[9] = {};
  mat3::MultiplyRowMajor(right_identity, m, kIdentity);
  CheckMatrixApprox(right_identity, m);
}

TEST_CASE("packed_mat3x3::MultiplyVec3 transforms a vector by the matrix", "[math][mat3x3]") {
  float out[3] = {};
  const float v[3] = {1.0f, 0.0f, 0.0f};
  // A 90-degree Z rotation should send the x-axis to the y-axis.
  float rotation[9] = {};
  mat3::BuildZRotationMatrix(rotation, static_cast<float>(M_PI_2));
  mat3::MultiplyVec3(out, v, rotation);
  CHECK(out[0] == Approx(0.0f).margin(0.0005));
  CHECK(out[1] == Approx(1.0f).margin(0.0005));
  CHECK(out[2] == Approx(0.0f).margin(0.0005));
}

TEST_CASE("packed_mat3x3::TransformVec3ByPackedMat3x3 mutates in_out_vec AND writes to out",
          "[math][mat3x3]") {
  // Documents a non-obvious dual-mutation: both the return-via-out AND
  // the in_out_vec parameter end up holding the transformed result.
  float rotation[9] = {};
  mat3::BuildZRotationMatrix(rotation, static_cast<float>(M_PI_2));

  float in_out_vec[3] = {1.0f, 0.0f, 0.0f};
  float out[3] = {};
  mat3::TransformVec3ByPackedMat3x3(out, in_out_vec, rotation);

  CHECK(out[0] == Approx(in_out_vec[0]));
  CHECK(out[1] == Approx(in_out_vec[1]));
  CHECK(out[2] == Approx(in_out_vec[2]));
  CHECK(in_out_vec[1] == Approx(1.0f).margin(0.0005)); // actually transformed, not left as input
}

TEST_CASE("packed_mat3x3::BuildZRotationMatrix at angle 0 is the identity", "[math][mat3x3]") {
  float m[9] = {};
  mat3::BuildZRotationMatrix(m, 0.0f);
  CheckMatrixApprox(m, kIdentity);
}

TEST_CASE(
    "packed_mat3x3::BuildPackedAxisAngleRotationMatrix3x3 about Z matches BuildZRotationMatrix",
    "[math][mat3x3]") {
  const float z_axis[3] = {0.0f, 0.0f, 1.0f};
  const float angle = 0.7f;

  float from_axis_angle[9] = {};
  mat3::BuildPackedAxisAngleRotationMatrix3x3(from_axis_angle, angle, z_axis, true);

  float from_z_rotation[9] = {};
  mat3::BuildZRotationMatrix(from_z_rotation, angle);

  CheckMatrixApprox(from_axis_angle, from_z_rotation);
}

TEST_CASE(
    "packed_mat3x3::BuildPackedBasisFromYawPitchRoll with only yaw matches BuildZRotationMatrix",
    "[math][mat3x3]") {
  const float yaw = 0.5f;
  float from_ypr[9] = {};
  mat3::BuildPackedBasisFromYawPitchRoll(from_ypr, yaw, 0.0f, 0.0f);

  float from_z_rotation[9] = {};
  mat3::BuildZRotationMatrix(from_z_rotation, yaw);

  CheckMatrixApprox(from_ypr, from_z_rotation);
}

TEST_CASE(
    "packed_mat3x3::ExtractYawPitchRoll round-trips BuildPackedBasisFromYawPitchRoll away from "
    "gimbal lock",
    "[math][mat3x3]") {
  const float yaw = 0.3f;
  const float pitch = 0.2f; // well away from +-pi/2
  const float roll = 0.4f;

  float m[9] = {};
  mat3::BuildPackedBasisFromYawPitchRoll(m, yaw, pitch, roll);

  float extracted_yaw = 0.0f;
  float extracted_pitch = 0.0f;
  float extracted_roll = 0.0f;
  CHECK(mat3::ExtractYawPitchRoll(m, &extracted_yaw, &extracted_pitch, &extracted_roll));
  CHECK(extracted_yaw == Approx(yaw).margin(0.001));
  CHECK(extracted_pitch == Approx(pitch).margin(0.001));
  CHECK(extracted_roll == Approx(roll).margin(0.001));
}

TEST_CASE(
    "packed_mat3x3::ExtractYawPitchRoll reports gimbal lock at pitch = +pi/2 and forces roll to 0",
    "[math][mat3x3]") {
  constexpr float kHalfPi = 1.5707964f;
  float m[9] = {};
  mat3::BuildPackedBasisFromYawPitchRoll(m, 0.2f, kHalfPi, 0.6f); // roll is lost to gimbal lock

  float yaw = 0.0f;
  float pitch = 0.0f;
  float roll = 99.0f;
  CHECK_FALSE(mat3::ExtractYawPitchRoll(m, &yaw, &pitch, &roll));
  CHECK(pitch == Approx(kHalfPi).margin(0.001));
  CHECK(roll == Approx(0.0f));
}
