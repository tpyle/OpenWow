#include "openwow/foundation/math/m2_camera_matrix.h"
#include "openwow/foundation/math/retail_camera_matrix.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using Catch::Approx;
using openwow::math::BuildM2CameraViewMatrix;
using openwow::math::BuildRetailCameraProjectionMatrix;
using openwow::math::BuildRetailCameraViewMatrix;

TEST_CASE("BuildRetailCameraViewMatrix falls back to the identity when eye == target "
          "(degenerate forward vector)",
          "[math][camera]") {
  const float eye[3] = {1.0f, 2.0f, 3.0f};
  const float target[3] = {1.0f, 2.0f, 3.0f}; // forward length == 0
  const float up[3] = {0.0f, 1.0f, 0.0f};

  float out[16];
  for (float &value : out)
    value = 999.0f; // sentinel
  BuildRetailCameraViewMatrix(eye, target, up, out);

  const float expected_identity[16] = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  for (int i = 0; i < 16; ++i) {
    CAPTURE(i);
    CHECK(out[i] == Approx(expected_identity[i]));
  }
}

TEST_CASE("BuildRetailCameraViewMatrix falls back to the identity for a degenerate up vector",
          "[math][camera]") {
  const float eye[3] = {0.0f, 0.0f, 0.0f};
  const float target[3] = {0.0f, 0.0f, 1.0f};
  const float up[3] = {0.0f, 0.0f, 0.0f}; // zero up vector

  float out[16] = {};
  BuildRetailCameraViewMatrix(eye, target, up, out);
  CHECK(out[0] == Approx(1.0f));
  CHECK(out[5] == Approx(1.0f));
  CHECK(out[10] == Approx(1.0f));
  CHECK(out[15] == Approx(1.0f));
  CHECK(out[12] == Approx(0.0f));
}

TEST_CASE("BuildRetailCameraViewMatrix builds a right-handed basis for a simple look-down-Z case",
          "[math][camera]") {
  const float eye[3] = {0.0f, 0.0f, 0.0f};
  const float target[3] = {0.0f, 0.0f, 1.0f};
  const float up[3] = {0.0f, 1.0f, 0.0f};

  float out[16] = {};
  BuildRetailCameraViewMatrix(eye, target, up, out);

  // Hand-derived: forward = (0,0,1), right = forward x up = (-1,0,0),
  // view_up = right x forward = (0,1,0).
  CHECK(out[0] == Approx(-1.0f).margin(0.0005)); // right.x
  CHECK(out[4] == Approx(0.0f).margin(0.0005));  // right.y
  CHECK(out[8] == Approx(0.0f).margin(0.0005));  // right.z
  CHECK(out[1] == Approx(0.0f).margin(0.0005));  // view_up.x
  CHECK(out[5] == Approx(1.0f).margin(0.0005));  // view_up.y
  CHECK(out[9] == Approx(0.0f).margin(0.0005));  // view_up.z
  CHECK(out[2] == Approx(0.0f).margin(0.0005));  // forward.x
  CHECK(out[6] == Approx(0.0f).margin(0.0005));  // forward.y
  CHECK(out[10] == Approx(1.0f).margin(0.0005)); // forward.z
  CHECK(out[12] == Approx(0.0f).margin(0.0005));
  CHECK(out[13] == Approx(0.0f).margin(0.0005));
  CHECK(out[14] == Approx(0.0f).margin(0.0005));
}

TEST_CASE("BuildRetailCameraViewMatrix's translation row is -eye projected onto the view basis",
          "[math][camera]") {
  const float eye[3] = {1.0f, 2.0f, 3.0f};
  const float target[3] = {1.0f, 2.0f, 4.0f}; // same forward direction as the case above
  const float up[3] = {0.0f, 1.0f, 0.0f};

  float out[16] = {};
  BuildRetailCameraViewMatrix(eye, target, up, out);

  // With the same right/view_up/forward basis as the origin-eye case:
  // out[12] = -(eye . right), out[13] = -(eye . view_up), out[14] = -(eye . forward).
  CHECK(out[12] == Approx(1.0f).margin(0.0005));  // -(1*-1 + 2*0 + 3*0)
  CHECK(out[13] == Approx(-2.0f).margin(0.0005)); // -(1*0 + 2*1 + 3*0)
  CHECK(out[14] == Approx(-3.0f).margin(0.0005)); // -(1*0 + 2*0 + 3*1)
}

TEST_CASE("BuildRetailCameraProjectionMatrix falls back to the identity for invalid parameters",
          "[math][camera]") {
  float out[16];
  for (float &value : out)
    value = 999.0f;

  SECTION("fov <= 0") {
    BuildRetailCameraProjectionMatrix(0.0f, 1.0f, 1.0f, 100.0f, out);
  }
  SECTION("fov >= pi") {
    BuildRetailCameraProjectionMatrix(3.2f, 1.0f, 1.0f, 100.0f, out);
  }
  SECTION("aspect <= 0") {
    BuildRetailCameraProjectionMatrix(1.0f, 0.0f, 1.0f, 100.0f, out);
  }
  SECTION("near >= far") {
    BuildRetailCameraProjectionMatrix(1.0f, 1.0f, 100.0f, 100.0f, out);
  }

  CHECK(out[0] == Approx(1.0f));
  CHECK(out[5] == Approx(1.0f));
  CHECK(out[10] == Approx(1.0f));
  CHECK(out[15] == Approx(1.0f));
  CHECK(out[1] == Approx(0.0f));
}

TEST_CASE("BuildRetailCameraProjectionMatrix matches the standard perspective formula",
          "[math][camera]") {
  const float fov = static_cast<float>(M_PI_2); // 90 degrees
  const float aspect = 1.0f;
  const float near_plane = 1.0f;
  const float far_plane = 100.0f;

  float out[16] = {};
  BuildRetailCameraProjectionMatrix(fov, aspect, near_plane, far_plane, out);

  const float inverse_tangent = 1.0f / std::tan(fov * 0.5f);
  const float depth_range = far_plane - near_plane;
  CHECK(out[0] == Approx(inverse_tangent / aspect).margin(0.0005));
  CHECK(out[5] == Approx(inverse_tangent).margin(0.0005));
  CHECK(out[10] == Approx((near_plane + far_plane) / depth_range).margin(0.0005));
  CHECK(out[11] == Approx(1.0f));
  CHECK(out[14] == Approx((-2.0f * near_plane * far_plane) / depth_range).margin(0.0005));
}

TEST_CASE("BuildM2CameraViewMatrix at roll=0 matches BuildRetailCameraViewMatrix with a "
          "straight-up reference vector",
          "[math][camera]") {
  const float position[3] = {0.0f, 0.0f, 0.0f};
  // Looking along +Y here (not +Z as an earlier version of this test
  // used): at roll=0, reference_up is (0, -sin(0), cos(0)) == (0, 0, 1).
  // A target of (0, 0, 1) would make forward and up parallel, which is
  // exactly the degenerate case flagged below -- picking a target
  // orthogonal to (0, 0, 1) keeps this test in the well-defined case.
  const float target[3] = {0.0f, 1.0f, 0.0f};

  float from_m2[16] = {};
  BuildM2CameraViewMatrix(position, target, 0.0f, from_m2);

  const float up[3] = {0.0f, 0.0f, 1.0f};
  float from_retail[16] = {};
  BuildRetailCameraViewMatrix(position, target, up, from_retail);

  for (int i = 0; i < 16; ++i) {
    CAPTURE(i);
    CHECK(from_m2[i] == Approx(from_retail[i]).margin(0.0005));
  }
}

TEST_CASE("BuildRetailCameraViewMatrix: FIXED -- forward parallel to up now falls back to "
          "the identity instead of producing NaN",
          "[math][camera]") {
  // This pins the fix for what was a real gameplay-reachable defect: the
  // only degeneracy guards used to be on forward/up each individually
  // being too short; there was no check for forward and up being
  // *parallel*. When they are, `right = forward x up` is the zero vector,
  // and dividing by its zero length produced +infinity and then NaN.
  // WorldCamera::GetViewMatrix() and its per-frame pose update both call
  // this with a fixed world-up reference vector (0,0,1) and a
  // forward/target that tracks player pitch -- a player pitching the
  // camera to look close to straight up or down pushes right_length_squared
  // toward zero. WorldCamera clamps pitch to ~89 degrees, just short of
  // exact parallel, but cinematic_player.cpp's calls to
  // BuildM2CameraViewMatrix (which forwards to this same function) use
  // camera data sampled from M2 cinematic tracks with no such clamp, so
  // this was reachable in practice, not just a theoretical edge case.
  //
  // A right_length_squared guard, matching the style of the existing
  // forward/up length guards, now makes this case fall back to the
  // identity (the value `out` is already pre-filled with at the top of
  // this function) instead of propagating NaN.
  const float eye[3] = {0.0f, 0.0f, 0.0f};
  const float target[3] = {0.0f, 0.0f, 1.0f};
  const float up[3] = {0.0f, 0.0f, 1.0f}; // parallel to forward

  float out[16] = {};
  BuildRetailCameraViewMatrix(eye, target, up, out);
  CHECK_FALSE(std::isnan(out[0]));
  CHECK(out[0] == Approx(1.0f));
  CHECK(out[5] == Approx(1.0f));
  CHECK(out[10] == Approx(1.0f));
  CHECK(out[15] == Approx(1.0f));
}

TEST_CASE("BuildRetailCameraViewMatrix falls back to the identity for near-parallel "
          "(not just exactly parallel) forward/up",
          "[math][camera]") {
  const float eye[3] = {0.0f, 0.0f, 0.0f};
  const float target[3] = {0.0f, 0.001f, 1.0f}; // ~0.057 degrees off vertical
  const float up[3] = {0.0f, 0.0f, 1.0f};

  float out[16] = {};
  BuildRetailCameraViewMatrix(eye, target, up, out);
  CHECK_FALSE(std::isnan(out[0]));
  CHECK_FALSE(std::isinf(out[0]));
}

TEST_CASE("BuildRetailCameraViewMatrix builds a real basis at the 89 degree pitch limit",
          "[math][camera]") {
  // The world camera clamps pitch to about 89 degrees; looking straight up
  // or down at the limit must still produce a view, not the identity.
  const float pitch = 1.5533430576324463f;
  const float eye[3] = {10.0f, 20.0f, 30.0f};
  for (const float sign : {1.0f, -1.0f}) {
    const float target[3] = {eye[0] + std::cos(pitch), eye[1],
                             eye[2] + sign * std::sin(pitch)};
    const float up[3] = {0.0f, 0.0f, 1.0f};
    float out[16] = {};
    BuildRetailCameraViewMatrix(eye, target, up, out);
    CHECK(out[2] == Approx(std::cos(pitch)).margin(1e-4));
    CHECK(out[10] == Approx(sign * std::sin(pitch)).margin(1e-4));
    const float right_length =
        std::sqrt(out[0] * out[0] + out[4] * out[4] + out[8] * out[8]);
    CHECK(right_length == Approx(1.0f).margin(1e-4));
    CHECK(out[12] != 0.0f);
  }
}
