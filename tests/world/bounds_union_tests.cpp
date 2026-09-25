#include "openwow/world/coordinates/bounds_union.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

using openwow::world::ExpandBoundsToCover;
using Box = std::array<float, 6>;

TEST_CASE("A zero placement box grows to cover its groups", "[world][bounds]") {
  Box placement{};  // all-zero geobox, as some transports have
  const std::vector<Box> groups{{-20, -5, -2, 10, 5, 12}, {5, -3, 0, 30, 3, 8}};
  ExpandBoundsToCover(placement, groups);
  CHECK(placement == Box{-20, -5, -2, 30, 5, 12});
}

TEST_CASE("A placement box that already covers its groups is unchanged", "[world][bounds]") {
  Box placement{-100, -100, -100, 100, 100, 100};
  const std::vector<Box> groups{{-20, -5, -2, 10, 5, 12}};
  ExpandBoundsToCover(placement, groups);
  CHECK(placement == Box{-100, -100, -100, 100, 100, 100});
}

TEST_CASE("No groups leaves the placement box alone", "[world][bounds]") {
  Box placement{1, 2, 3, 4, 5, 6};
  ExpandBoundsToCover(placement, std::vector<Box>{});
  CHECK(placement == Box{1, 2, 3, 4, 5, 6});
}
