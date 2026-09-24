#include "openwow/net/wotlk/realm_list.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>

using openwow::net::wotlk::RealmType;
using openwow::net::wotlk::RealmTypeToString;

TEST_CASE("RealmType values match the realm list wire encoding", "[net][realm_list]") {
  CHECK(static_cast<std::uint32_t>(RealmType::kNormal) == 0);
  CHECK(static_cast<std::uint32_t>(RealmType::kPvP) == 1);
  CHECK(static_cast<std::uint32_t>(RealmType::kRP) == 6);
  CHECK(static_cast<std::uint32_t>(RealmType::kRPPvP) == 8);
}

TEST_CASE("RealmTypeToString names each wire value", "[net][realm_list]") {
  CHECK(RealmTypeToString(static_cast<RealmType>(0)) == "Normal");
  CHECK(RealmTypeToString(static_cast<RealmType>(1)) == "PvP");
  CHECK(RealmTypeToString(static_cast<RealmType>(6)) == "RP");
  CHECK(RealmTypeToString(static_cast<RealmType>(8)) == "RP-PvP");
}

TEST_CASE("RealmTypeToString reports unlisted values as unknown", "[net][realm_list]") {
  CHECK(RealmTypeToString(static_cast<RealmType>(4)) == "Unknown");
  CHECK(RealmTypeToString(static_cast<RealmType>(5)) == "Unknown");
  CHECK(RealmTypeToString(static_cast<RealmType>(255)) == "Unknown");
}
