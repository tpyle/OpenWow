#include "openwow/render/scene/shadow_data.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

using openwow::render::RenderMatrix4x4;
using openwow::render::SnapLightViewToTexelGrid;

RenderMatrix4x4 ViewWithTranslation(float x, float y, float z) {
  RenderMatrix4x4 view{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1};
  return view;
}

bool IsOnGrid(float value, float texel) {
  const float steps = value / texel;
  return std::fabs(steps - std::round(steps)) < 1e-3f;
}

}  // namespace

TEST_CASE("Light view translation lands on the texel grid", "[render][shadow]") {
  // 40-unit half extent at 1024 texels: 0.078125 units per texel.
  auto view = ViewWithTranslation(-8812.3456f, 641.0101f, 2000.5f);
  SnapLightViewToTexelGrid(view, 40.0f, 1024u);
  constexpr float kTexel = 80.0f / 1024.0f;
  CHECK(IsOnGrid(view[12], kTexel));
  CHECK(IsOnGrid(view[13], kTexel));
  CHECK(view[12] == Catch::Approx(-8812.3456f).margin(kTexel / 2.0f));
  CHECK(view[13] == Catch::Approx(641.0101f).margin(kTexel / 2.0f));
  CHECK(view[14] == 2000.5f);
}

TEST_CASE("Sub-texel moves snap to the same light view", "[render][shadow]") {
  constexpr float kTexel = 40.0f / 1024.0f;
  auto a = ViewWithTranslation(100.0f * kTexel, 7.0f * kTexel, 0.0f);
  auto b = ViewWithTranslation(100.3f * kTexel, 6.8f * kTexel, 0.0f);
  SnapLightViewToTexelGrid(a, 20.0f, 1024u);
  SnapLightViewToTexelGrid(b, 20.0f, 1024u);
  CHECK(a[12] == Catch::Approx(b[12]));
  CHECK(a[13] == Catch::Approx(b[13]));
}

TEST_CASE("Rotation part of the light view is left alone", "[render][shadow]") {
  RenderMatrix4x4 view{0.6f, 0.8f, 0, 0, -0.8f, 0.6f, 0, 0, 0, 0, 1, 0, 1.23f, 4.56f, 7.0f, 1};
  const RenderMatrix4x4 original = view;
  SnapLightViewToTexelGrid(view, 20.0f, 2048u);
  for (int i = 0; i < 12; ++i) {
    CHECK(view[i] == original[i]);
  }
}

TEST_CASE("Invalid snap inputs leave the view unchanged", "[render][shadow]") {
  const auto original = ViewWithTranslation(1.2345f, 6.789f, 0.0f);
  auto zero_resolution = original;
  SnapLightViewToTexelGrid(zero_resolution, 20.0f, 0u);
  CHECK(zero_resolution == original);
  auto zero_extent = original;
  SnapLightViewToTexelGrid(zero_extent, 0.0f, 1024u);
  CHECK(zero_extent == original);
  auto nan_extent = original;
  SnapLightViewToTexelGrid(nan_extent, std::nanf(""), 1024u);
  CHECK(nan_extent == original);
}
