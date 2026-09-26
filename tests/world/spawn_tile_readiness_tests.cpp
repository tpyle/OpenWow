#include "openwow/world/streaming/spawn_tile_readiness.h"

#include <catch2/catch_test_macros.hpp>

using openwow::world::IsSpawnTileSettled;

TEST_CASE("IsSpawnTileSettled", "[world][streaming]") {
  SECTION("existing tile waits until it is loaded") {
    CHECK_FALSE(IsSpawnTileSettled(false, true, false));
    CHECK(IsSpawnTileSettled(false, true, true));
  }
  SECTION("tile missing from the WDT never blocks") {
    CHECK(IsSpawnTileSettled(false, false, false));
    CHECK(IsSpawnTileSettled(false, false, true));
  }
  SECTION("global-WMO maps have no terrain tiles to wait for") {
    CHECK(IsSpawnTileSettled(true, true, false));
    CHECK(IsSpawnTileSettled(true, false, false));
  }
}
