#include "openwow/ui/font_pixel_height.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using openwow::ui::ResolveFontPixelHeight;

TEST_CASE("Stored font sizes round to the drawn pixel height", "[ui][font]") {
  CHECK(ResolveFontPixelHeight(14.0) == 14);
  CHECK(ResolveFontPixelHeight(13.999999) == 14);
  CHECK(ResolveFontPixelHeight(14.0000001) == 14);
  CHECK(ResolveFontPixelHeight(12.5) == 13);
  CHECK(ResolveFontPixelHeight(12.4) == 12);
}

TEST_CASE("Missing or invalid font sizes give no height", "[ui][font]") {
  CHECK(ResolveFontPixelHeight(0.0) == 0);
  CHECK(ResolveFontPixelHeight(-3.0) == 0);
  CHECK(ResolveFontPixelHeight(std::numeric_limits<double>::quiet_NaN()) == 0);
  CHECK(ResolveFontPixelHeight(std::numeric_limits<double>::infinity()) == 0);
}
