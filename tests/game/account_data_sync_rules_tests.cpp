#include "openwow/game/account_data_sync_rules.h"

#include <catch2/catch_test_macros.hpp>

namespace sync = openwow::game::account_data_sync;

TEST_CASE("ApplyServerTimesForMask only updates masked slots",
          "[account_data]") {
  std::array<std::uint32_t, 8> synchronized{100, 101, 102, 103,
                                            104, 105, 106, 107};
  const std::array<std::uint32_t, 8> incoming{0, 201, 0, 203, 0, 205, 0, 207};

  SECTION("per-character mask leaves account-wide slots alone") {
    sync::ApplyServerTimesForMask(synchronized, incoming, 0xAAu);
    CHECK(synchronized ==
          std::array<std::uint32_t, 8>{100, 201, 102, 203, 104, 205, 106, 207});
  }
  SECTION("empty mask changes nothing") {
    sync::ApplyServerTimesForMask(synchronized, incoming, 0u);
    CHECK(synchronized ==
          std::array<std::uint32_t, 8>{100, 101, 102, 103, 104, 105, 106, 107});
  }
  SECTION("full mask copies everything, including zero times") {
    sync::ApplyServerTimesForMask(synchronized, incoming, 0xFFu);
    CHECK(synchronized == incoming);
  }
  SECTION("bits above slot 7 are ignored") {
    sync::ApplyServerTimesForMask(synchronized, incoming, 0xFF00u);
    CHECK(synchronized[0] == 100);
    CHECK(synchronized[7] == 107);
  }
}

TEST_CASE("ChooseDiskLoadMerge", "[account_data]") {
  using sync::DiskLoadMerge;

  SECTION("newer downloaded payload wins over the disk copy") {
    CHECK(sync::ChooseDiskLoadMerge({500, 500}, {400, 400}) ==
          DiskLoadMerge::kAdoptServerPayload);
  }
  SECTION("downloaded payload wins when nothing is on disk") {
    CHECK(sync::ChooseDiskLoadMerge({500, 500}, {0, 0}) ==
          DiskLoadMerge::kAdoptServerPayload);
  }
  SECTION("same-age payload keeps the disk copy") {
    CHECK(sync::ChooseDiskLoadMerge({400, 400}, {400, 400}) ==
          DiskLoadMerge::kKeepDisk);
  }
  SECTION("older payload keeps the disk copy") {
    CHECK(sync::ChooseDiskLoadMerge({300, 300}, {400, 400}) ==
          DiskLoadMerge::kKeepDisk);
  }
  SECTION("server time known but payload not yet received") {
    CHECK(sync::ChooseDiskLoadMerge({0, 600}, {400, 400}) ==
          DiskLoadMerge::kAdoptServerTime);
  }
  SECTION("stale server time does not override the disk") {
    CHECK(sync::ChooseDiskLoadMerge({0, 300}, {400, 400}) ==
          DiskLoadMerge::kKeepDisk);
  }
  SECTION("nothing received from the server") {
    CHECK(sync::ChooseDiskLoadMerge({0, 0}, {400, 400}) ==
          DiskLoadMerge::kKeepDisk);
  }
  SECTION("payload older than its server time is not treated as downloaded") {
    CHECK(sync::ChooseDiskLoadMerge({450, 600}, {400, 400}) ==
          DiskLoadMerge::kAdoptServerTime);
  }
}
