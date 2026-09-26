#include "openwow/world/wmo/wmo_visibility.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Synthetic two-room WMO: an "outside" room (group 0) and a vaulted passage
// (group 1) joined by a doorway in the plane y = 0 (x -4..4, z 1..11). The
// numbers are made up; the layout mirrors a street opening onto an arch.

namespace {

using openwow::world::ComputeVisibleWmoGroups;
using openwow::world::Frustum;
using openwow::world::Matrix4;
using openwow::world::NearClipCornerRadius;
using openwow::world::WmoVisibilityData;
using openwow::world::WmoVisibilityMask;
using openwow::world::WmoVisibilityWorkspace;
namespace wmo = openwow::data::wmo;

constexpr std::uint32_t kStreetFlags = 0x2000u | 0x40u;  // interior, exterior-lit

WmoVisibilityData BuildStreetAndArch() {
  wmo::WmoRoot root;
  root.groupInfos = {
      {kStreetFlags, {-40.0f, -40.0f, -5.0f}, {40.0f, 0.0f, 30.0f}, 0},
      {kStreetFlags, {-5.0f, 0.0f, -2.0f}, {5.0f, 20.0f, 12.0f}, 0},
  };
  root.portalVertices = {
      {-4.0f, 0.0f, 1.0f}, {4.0f, 0.0f, 1.0f}, {4.0f, 0.0f, 11.0f}, {-4.0f, 0.0f, 11.0f}};
  root.portals = {{0u, 4u, {0.0f, -1.0f, 0.0f}, 0.0f}};
  // Street sees the portal from its positive side, the arch from its negative.
  root.portalRefs = {{0u, 1u, 1, 0u}, {0u, 0u, -1, 0u}};

  std::vector<wmo::WmoGroup> groups(2);
  for (std::size_t index = 0; index < groups.size(); ++index) {
    auto& header = groups[index].header;
    header.flags = kStreetFlags;
    header.portalStart = static_cast<std::uint16_t>(index);
    header.portalCount = 1u;
  }
  return WmoVisibilityData::Build(root, groups);
}

using Vec = std::array<float, 3>;

Vec Normalize(const Vec& v) {
  const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  return {v[0] / length, v[1] / length, v[2] / length};
}

Vec Cross(const Vec& a, const Vec& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}

float Dot(const Vec& a, const Vec& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// Row-vector, left-handed look-at and zero-to-one perspective (bx layout).
Matrix4 LookAt(const Vec& eye, const Vec& forward, const Vec& up_hint) {
  const Vec z = Normalize(forward);
  const Vec x = Normalize(Cross(up_hint, z));
  const Vec y = Cross(z, x);
  return {x[0], y[0], z[0], 0.0f, x[1], y[1], z[1], 0.0f, x[2], y[2], z[2], 0.0f,
          -Dot(x, eye), -Dot(y, eye), -Dot(z, eye), 1.0f};
}

Matrix4 Perspective(float vertical_fov, float aspect, float near_clip, float far_clip) {
  const float y_scale = 1.0f / std::tan(vertical_fov * 0.5f);
  const float x_scale = y_scale / aspect;
  const float depth = far_clip / (far_clip - near_clip);
  return {x_scale, 0.0f, 0.0f, 0.0f, 0.0f, y_scale, 0.0f, 0.0f,
          0.0f, 0.0f, depth, 1.0f, 0.0f, 0.0f, -near_clip * depth, 0.0f};
}

Matrix4 Multiply(const Matrix4& a, const Matrix4& b) {
  Matrix4 out{};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      for (int inner = 0; inner < 4; ++inner) {
        out[row * 4 + column] += a[row * 4 + inner] * b[inner * 4 + column];
      }
    }
  }
  return out;
}

struct VisibilityResult {
  WmoVisibilityMask mask;
};

VisibilityResult Visible(const Vec& eye, const Vec& forward, float near_clip_radius) {
  static const WmoVisibilityData visibility = BuildStreetAndArch();
  const Matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  const Matrix4 projection = Perspective(1.0f, 16.0f / 9.0f, 0.4f, 1000.0f);
  const Matrix4 view_projection =
      Multiply(LookAt(eye, forward, {0.0f, 0.0f, 1.0f}), projection);
  Frustum frustum;
  frustum.ExtractFromViewProj(std::span<const float, 16>{view_projection});
  const std::array<float, 6> placement_bounds{-40, -40, -5, 40, 20, 30};
  const std::vector<std::array<float, 6>> group_bounds{
      {-40, -40, -5, 40, 0, 30}, {-5, 0, -2, 5, 20, 12}};
  const std::array<std::uint16_t, 1> seeds{0u};
  WmoVisibilityWorkspace workspace;
  VisibilityResult result;
  ComputeVisibleWmoGroups(visibility, identity, view_projection, placement_bounds,
                          group_bounds, frustum, eye[0], eye[1], eye[2], Normalize(forward),
                          seeds, workspace, result.mask, nullptr, nullptr, nullptr,
                          openwow::world::WmoTraversalLanes::kCamera, nullptr, false,
                          near_clip_radius);
  return result;
}

bool GroupVisible(const VisibilityResult& result, std::size_t group) {
  return group < result.mask.size() && result.mask[group] != 0u;
}

// Millimetres outside the doorway plane and 0.39 below its sill, looking up
// and into the vault (a camera pressed into the step at an arch mouth). From
// the eye point the doorway is edge-on and clips away entirely.
constexpr Vec kEyeAtSill{0.0f, -0.0034f, 0.61f};
constexpr Vec kLookUpIntoArch{-0.087f, 0.79f, 0.61f};

}  // namespace

TEST_CASE("NearClipCornerRadius covers the near rectangle corners", "[world][wmo_portal]") {
  const Matrix4 projection = Perspective(1.0f, 16.0f / 9.0f, 0.4f, 1000.0f);
  const float tan_y = std::tan(0.5f);
  const float tan_x = tan_y * 16.0f / 9.0f;
  CHECK(NearClipCornerRadius(projection, 0.4f) ==
        Catch::Approx(0.4f * std::sqrt(1.0f + tan_x * tan_x + tan_y * tan_y)));
  CHECK(NearClipCornerRadius(projection, 0.0f) == 0.0f);
  CHECK(NearClipCornerRadius(Matrix4{}, 0.4f) == 0.0f);
}

TEST_CASE("A room seen through a portal from well inside the street is drawn",
          "[world][wmo_portal]") {
  const auto result = Visible({0.0f, -6.0f, 5.0f}, {0.0f, 1.0f, 0.0f}, 0.0f);
  CHECK(GroupVisible(result, 0));
  CHECK(GroupVisible(result, 1));
}

TEST_CASE("A room behind the camera's back is not drawn", "[world][wmo_portal]") {
  const auto result = Visible({0.0f, -6.0f, 5.0f}, {0.0f, -1.0f, 0.0f}, 0.5f);
  CHECK(GroupVisible(result, 0));
  CHECK_FALSE(GroupVisible(result, 1));
}

TEST_CASE("An edge-on portal at the eye is lost without near-clip tolerance",
          "[world][wmo_portal]") {
  CHECK_FALSE(GroupVisible(Visible(kEyeAtSill, kLookUpIntoArch, 0.0f), 1));
  // A step back from the plane the same view does reach the vault.
  CHECK(GroupVisible(Visible({0.0f, -0.28f, 0.61f}, kLookUpIntoArch, 0.0f), 1));
}

TEST_CASE("A portal within the near clip rectangle keeps its room drawn",
          "[world][wmo_portal]") {
  const Matrix4 projection = Perspective(1.0f, 16.0f / 9.0f, 0.4f, 1000.0f);
  const float radius = NearClipCornerRadius(projection, 0.4f);
  CHECK(GroupVisible(Visible(kEyeAtSill, kLookUpIntoArch, radius), 1));
  // Same spot, a hair past the plane on the arch side.
  CHECK(GroupVisible(Visible({0.0f, 0.0034f, 0.61f}, kLookUpIntoArch, radius), 1));
}

TEST_CASE("Near-clip tolerance does not reach a portal far from the eye",
          "[world][wmo_portal]") {
  const Matrix4 projection = Perspective(1.0f, 16.0f / 9.0f, 0.4f, 1000.0f);
  const float radius = NearClipCornerRadius(projection, 0.4f);
  // Standing beside the doorway, outside its span, looking away along the wall.
  const auto result = Visible({12.0f, -0.003f, 0.6f}, {1.0f, 0.0f, 0.3f}, radius);
  CHECK_FALSE(GroupVisible(result, 1));
}

TEST_CASE("Exterior-lane seeds include open-air groups seen from open air",
          "[world][wmo_portal]") {
  using openwow::world::IsExteriorLaneSeedGroup;
  constexpr std::uint32_t kExterior = 0x8u;
  constexpr std::uint32_t kAlwaysDraw = 0x10000u;
  // Stormwind's gryphon-roost wall tops: indoor, lit as exterior.
  constexpr std::uint32_t kOpenAirIndoor = 0x2000u | 0x40u;
  constexpr std::uint32_t kPlainIndoor = 0x2000u;

  CHECK(IsExteriorLaneSeedGroup(kExterior, false));
  CHECK(IsExteriorLaneSeedGroup(kExterior, true));
  CHECK_FALSE(IsExteriorLaneSeedGroup(kOpenAirIndoor, false));
  CHECK(IsExteriorLaneSeedGroup(kOpenAirIndoor, true));
  CHECK(IsExteriorLaneSeedGroup(kPlainIndoor | 0x100u, true));    // exterior sky
  CHECK(IsExteriorLaneSeedGroup(kPlainIndoor | 0x40000u, true));  // skybox
  CHECK_FALSE(IsExteriorLaneSeedGroup(kPlainIndoor, true));
  CHECK_FALSE(IsExteriorLaneSeedGroup(kExterior | kAlwaysDraw, false));
  CHECK_FALSE(IsExteriorLaneSeedGroup(kOpenAirIndoor | kAlwaysDraw, true));
}
