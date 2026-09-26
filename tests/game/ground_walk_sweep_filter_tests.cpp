#include "openwow/game/ground_walk.h"

#include <catch2/catch_test_macros.hpp>

using openwow::game::IsFacetIgnoredBySweep;
using openwow::game::MovementCollisionBody;
using openwow::game::MovementCollisionFacet;

TEST_CASE("IsFacetIgnoredBySweep skips only the ignored owner",
          "[ground_walk]") {
  MovementCollisionBody body;
  MovementCollisionFacet carrier_facet;
  carrier_facet.owner_guid = 0x1F40000000000009ull;
  MovementCollisionFacet other_facet;
  other_facet.owner_guid = 0x1F4000000000000Aull;
  MovementCollisionFacet terrain_facet;  // owner_guid 0: static world

  SECTION("nothing is ignored by default") {
    CHECK_FALSE(IsFacetIgnoredBySweep(body, carrier_facet));
    CHECK_FALSE(IsFacetIgnoredBySweep(body, other_facet));
    CHECK_FALSE(IsFacetIgnoredBySweep(body, terrain_facet));
  }
  SECTION("the carrier's facets are ignored, others are not") {
    body.ignored_owner_guid = carrier_facet.owner_guid;
    CHECK(IsFacetIgnoredBySweep(body, carrier_facet));
    CHECK_FALSE(IsFacetIgnoredBySweep(body, other_facet));
    CHECK_FALSE(IsFacetIgnoredBySweep(body, terrain_facet));
  }
}
