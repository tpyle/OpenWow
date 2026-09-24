#include "openwow/data/wdb_persistence.h"

#include <catch2/catch_test_macros.hpp>

using openwow::data::GetCacheDirectory;

TEST_CASE("GetCacheDirectory uses a per-locale folder under the standard layout",
          "[data][wdb]") {
  CHECK(GetCacheDirectory(true, "enUS") == "Cache/WDB/enUS");
  CHECK(GetCacheDirectory(true, "enGB") == "Cache/WDB/enGB");
  CHECK(GetCacheDirectory(true, "deDE") == "Cache/WDB/deDE");
}

TEST_CASE("GetCacheDirectory uses the flat WDB folder without the standard layout",
          "[data][wdb]") {
  CHECK(GetCacheDirectory(false, "enUS") == "WDB");
  CHECK(GetCacheDirectory(false, "frFR") == "WDB");
}
