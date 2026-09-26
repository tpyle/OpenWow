#include "openwow/game/activities/lfg/application/lfg_manager.h"

#include "openwow/game/activities/lfg/rules/lfg_dungeon_rules.h"

#include "payload_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>

using openwow::game::LfgManager;
using openwow::game::LfgSearchSortKey;
using Bytes = openwow::test::PayloadBuilder;
namespace lfg = openwow::game::lfg;

namespace {

bool Feed(bool (LfgManager::*handler)(const std::uint8_t*, std::size_t), LfgManager& manager,
          const Bytes& bytes) {
  return (manager.*handler)(bytes.data(), bytes.size());
}

Bytes Proposal(std::uint32_t id, bool silent) {
  return Bytes().u32(0x06000105).u8(1).u32(id).u32(0).u8(silent ? 1 : 0).u8(1)
      .u32(2).u8(1).u8(0).u8(1).u8(0).u8(0);
}

}  // namespace

TEST_CASE("LfgManager shows each new proposal once unless silent", "[lfg][manager]") {
  LfgManager lfg_state;
  REQUIRE(Feed(&LfgManager::HandleLfgProposalUpdate, lfg_state, Proposal(5, false)));
  CHECK(lfg_state.ConsumeProposalShowPending());
  CHECK_FALSE(lfg_state.ConsumeProposalShowPending());

  // Another update for the same proposal doesn't show it again.
  REQUIRE(Feed(&LfgManager::HandleLfgProposalUpdate, lfg_state, Proposal(5, false)));
  CHECK_FALSE(lfg_state.ConsumeProposalShowPending());

  REQUIRE(Feed(&LfgManager::HandleLfgProposalUpdate, lfg_state, Proposal(6, true)));
  CHECK_FALSE(lfg_state.ConsumeProposalShowPending());

  // Entering the world forgets the last proposal id.
  lfg_state.ResetProposalEventGateForPlayerEnterWorld();
  REQUIRE(Feed(&LfgManager::HandleLfgProposalUpdate, lfg_state, Proposal(6, false)));
  CHECK(lfg_state.ConsumeProposalShowPending());

  lfg_state.ApplyProposalResponse(true);
  REQUIRE(lfg_state.proposal());
  CHECK(lfg_state.proposal()->players[0].has_answered);
  CHECK(lfg_state.proposal()->players[0].has_accepted);
  lfg_state.ApplyProposalResponse(false);
  CHECK_FALSE(lfg_state.proposal());
}

TEST_CASE("LfgManager keeps state when a payload doesn't decode", "[lfg][manager]") {
  LfgManager lfg_state;
  REQUIRE(Feed(&LfgManager::HandleLfgTeleportDenied, lfg_state, Bytes().u32(3)));
  CHECK_FALSE(Feed(&LfgManager::HandleLfgTeleportDenied, lfg_state, Bytes().u8(1)));
  CHECK(lfg_state.teleport_error() == 3u);

  REQUIRE(Feed(&LfgManager::HandleLfgQueueStatus, lfg_state,
               Bytes().u32(1).i32(1).i32(2).i32(3).i32(4).i32(5).u8(0).u8(0).u8(0).u32(9)));
  CHECK_FALSE(Feed(&LfgManager::HandleLfgQueueStatus, lfg_state, Bytes().u32(2)));
  REQUIRE(lfg_state.queue_status());
  CHECK(lfg_state.queue_status()->queued_time == 9u);
}

TEST_CASE("LfgManager drops the previous reward even on a malformed reward", "[lfg][manager]") {
  LfgManager lfg_state;
  const auto reward = Bytes().u32(1).u32(2).u8(0).u32(0).u32(10).u32(20).u32(0).u32(0).u8(0);
  REQUIRE(Feed(&LfgManager::HandleLfgPlayerReward, lfg_state, reward));
  REQUIRE(lfg_state.player_reward());
  CHECK_FALSE(Feed(&LfgManager::HandleLfgPlayerReward, lfg_state, Bytes().u32(1)));
  CHECK_FALSE(lfg_state.player_reward());
}

TEST_CASE("LfgManager keeps raw info payloads even when they don't decode", "[lfg][manager]") {
  LfgManager lfg_state;
  CHECK_FALSE(Feed(&LfgManager::HandleLfgPlayerInfo, lfg_state, Bytes().u8(1)));
  CHECK(lfg_state.lfg_player_info_blob().size() == 1u);
  CHECK_FALSE(lfg_state.has_player_dungeon_info());
}

TEST_CASE("LfgManager joinability from player and party locks", "[lfg][manager]") {
  LfgManager lfg_state;
  const auto heroic = lfg::PackDungeonId(10, 5);
  const auto raid = lfg::PackDungeonId(11, 2);
  const auto dungeon = lfg::PackDungeonId(12, 1);
  CHECK(lfg_state.IsDungeonJoinable(heroic));  // nothing known yet

  // Player info: heroic locked (reason 2), dungeon open.
  REQUIRE(Feed(&LfgManager::HandleLfgPlayerInfo, lfg_state,
               Bytes().u8(1).u32(dungeon).u8(0).u32(0).u32(0).u32(0).u32(0).u8(0)
                   .u32(1).u32(heroic).u32(2)));
  CHECK_FALSE(lfg_state.IsDungeonJoinable(heroic));
  CHECK(lfg_state.IsDungeonJoinable(dungeon));
  CHECK(lfg_state.FindPlayerLockReason(10u) == 2u);
  CHECK(lfg_state.HasUnlockedPlayerDungeon(dungeon));
  CHECK_FALSE(lfg_state.HasUnlockedPlayerDungeon(heroic));

  // A party member locked out of the dungeon and the raid: only the dungeon
  // becomes unjoinable (party locks don't apply to raids).
  REQUIRE(Feed(&LfgManager::HandleLfgPartyInfo, lfg_state,
               Bytes().u8(1).u64(0xA).u32(2).u32(dungeon).u32(1).u32(raid).u32(1)));
  CHECK_FALSE(lfg_state.IsDungeonJoinable(dungeon));
  CHECK(lfg_state.IsDungeonJoinable(raid));
  CHECK(lfg_state.GetLfdLockPlayerCount() == 2u);  // the player and one member
}

TEST_CASE("LfgManager random dungeon choice uses its lock state", "[lfg][manager]") {
  LfgManager lfg_state;
  const std::vector<lfg::RandomDungeonCandidate> candidates{
      {.dungeon_id = 258, .type_id = 6, .expansion_level = 0, .target_level_average = 20},
      {.dungeon_id = 262, .type_id = 6, .expansion_level = 2, .target_level_average = 80},
  };
  CHECK(lfg_state.GetBestRandomDungeonId(candidates) == 262u);
  REQUIRE(Feed(&LfgManager::HandleLfgPlayerInfo, lfg_state,
               Bytes().u8(0).u32(1).u32(lfg::PackDungeonId(262, 6)).u32(1)));
  CHECK(lfg_state.GetBestRandomDungeonId(candidates) == 258u);
  CHECK(lfg_state.GetAvailableRandomDungeonIds(candidates) ==
        std::vector<std::uint32_t>{258, 262});
}

TEST_CASE("LfgManager disabled clears everything", "[lfg][manager]") {
  LfgManager lfg_state;
  REQUIRE(Feed(&LfgManager::HandleLfgTeleportDenied, lfg_state, Bytes().u32(3)));
  REQUIRE(lfg_state.HandleLfgDisabled(nullptr, 0));
  CHECK(lfg_state.lfg_disabled());
  CHECK(lfg_state.teleport_error() == 0u);
}

TEST_CASE("LfgManager sort key promotion", "[lfg][manager]") {
  LfgManager lfg_state;
  CHECK(lfg_state.search_sort_order().front() == std::pair{LfgSearchSortKey::kZone, false});
  lfg_state.PromoteSearchSortKey(LfgSearchSortKey::kName);
  CHECK(lfg_state.search_sort_order()[0] == std::pair{LfgSearchSortKey::kName, false});
  CHECK(lfg_state.search_sort_order()[1] == std::pair{LfgSearchSortKey::kZone, false});
  CHECK(lfg_state.search_sort_order()[4] == std::pair{LfgSearchSortKey::kTank, false});
  // Promoting the front key again flips its direction.
  lfg_state.PromoteSearchSortKey(LfgSearchSortKey::kName);
  CHECK(lfg_state.search_sort_order()[0] == std::pair{LfgSearchSortKey::kName, true});
}

namespace {

constexpr std::uint32_t kSearchType = 2;
constexpr std::uint32_t kSearchDungeon = 0xA1;
constexpr std::uint32_t kPackedSearch = (kSearchType << 24) | kSearchDungeon;

// A full-replace browse list of standalone players, each with level and
// area set.
Bytes PlayerList(const std::vector<std::pair<std::uint64_t, std::uint8_t>>& players) {
  Bytes b;
  b.u32(kSearchType).u32(kSearchDungeon).u8(0);  // no delete list: replace
  b.u32(0).u32(0);                                // no groups
  b.u32(static_cast<std::uint32_t>(players.size())).u32(static_cast<std::uint32_t>(players.size()));
  for (const auto& [guid, level] : players) {
    b.u64(guid).u32(0x1 | 0x20).u8(level).u8(0).u8(0).u8(0).u8(0).u8(0);
    for (int i = 0; i < 6; ++i) b.u32(0);
    b.u32(0).u32(0);  // two floats
    for (int i = 0; i < 5; ++i) b.u32(0);
    b.u32(0);         // float
    for (int i = 0; i < 6; ++i) b.u32(0);
    b.u32(static_cast<std::uint32_t>(guid));  // area id = guid, for the zone sort
  }
  return b;
}

std::vector<std::uint64_t> PlayerOrder(const LfgManager& lfg_state) {
  std::vector<std::uint64_t> order;
  for (const auto* player : lfg_state.StandaloneSearchPlayers()) order.push_back(player->guid);
  return order;
}

}  // namespace

TEST_CASE("LfgManager browse list: only the joined search is applied", "[lfg][manager]") {
  LfgManager lfg_state;
  const auto list = PlayerList({{1, 80}, {2, 70}});
  // Not browsing that search: ignored but accepted.
  CHECK(Feed(&LfgManager::HandleUpdateLfgList, lfg_state, list));
  CHECK_FALSE(lfg_state.has_search_results());

  lfg_state.StartSearchBrowse(kPackedSearch);
  const auto generation = lfg_state.published_search_generation();
  REQUIRE(Feed(&LfgManager::HandleUpdateLfgList, lfg_state, list));
  CHECK(lfg_state.has_search_results());
  CHECK(lfg_state.active_search_id() == kPackedSearch);
  CHECK(lfg_state.published_search_generation() != generation);
  CHECK(PlayerOrder(lfg_state) == std::vector<std::uint64_t>{1, 2});
  CHECK(lfg_state.search_result_total_count() == 2u);

  // The server ending the search drops the results.
  REQUIRE(Feed(&LfgManager::HandleLfgUpdateSearch, lfg_state, Bytes().u8(0)));
  CHECK(lfg_state.joined_search_id() == 0u);
  CHECK_FALSE(lfg_state.has_search_results());
}

TEST_CASE("LfgManager browse list name queries go out once per player", "[lfg][manager]") {
  LfgManager lfg_state;
  lfg_state.StartSearchBrowse(kPackedSearch);
  REQUIRE(Feed(&LfgManager::HandleUpdateLfgList, lfg_state, PlayerList({{1, 80}, {2, 70}, {3, 60}})));

  std::vector<std::uint64_t> sent;
  const auto send = [&sent](std::uint64_t guid) { sent.push_back(guid); };
  lfg_state.QueueMissingSearchPlayerNameQueries([](std::uint64_t guid) { return guid == 2; },
                                                send);
  CHECK(sent == std::vector<std::uint64_t>{1, 3});
  // Nothing new to ask about.
  lfg_state.QueueMissingSearchPlayerNameQueries([](std::uint64_t) { return false; }, send);
  CHECK(sent.size() == 2u);
  // True only when the last outstanding query is answered.
  CHECK_FALSE(lfg_state.ResolvePendingSearchPlayerNameQuery(1));
  CHECK_FALSE(lfg_state.ResolvePendingSearchPlayerNameQuery(1));  // not pending any more
  CHECK_FALSE(lfg_state.ResolvePendingSearchPlayerNameQuery(2));  // never asked
  CHECK(lfg_state.ResolvePendingSearchPlayerNameQuery(3));
}

TEST_CASE("LfgManager browse list sorting uses the injected lookups", "[lfg][manager]") {
  LfgManager lfg_state;
  lfg_state.StartSearchBrowse(kPackedSearch);
  REQUIRE(Feed(&LfgManager::HandleUpdateLfgList, lfg_state,
               PlayerList({{1, 70}, {2, 80}, {3, 75}})));

  const std::map<std::uint64_t, std::string> names{{1, "Carl"}, {2, "alice"}, {3, "Bob"}};
  const std::map<std::uint32_t, std::string> zones{{1, "Zangarmarsh"}, {2, "Dalaran"},
                                                   {3, "Dalaran"}};
  int player_lookups = 0;
  lfg::SearchSortContext context;
  context.find_player = [&](std::uint64_t guid) -> std::optional<lfg::SearchPlayerIdentity> {
    ++player_lookups;
    return lfg::SearchPlayerIdentity{.name = names.at(guid), .class_id = 1};
  };
  context.area_name = [&](std::uint32_t area) -> std::optional<std::string_view> {
    return zones.at(area);
  };
  context.class_name = [](std::uint8_t) -> std::optional<std::string_view> { return "Warrior"; };
  context.compare_labels = [](const char* l, const char* r) { return std::string(l).compare(r); };
  context.compare_player_names = [](const char* l, const char* r) {
    return std::string(l).compare(r);
  };

  lfg_state.PromoteSearchSortKey(LfgSearchSortKey::kLevel);
  lfg_state.ResortSearchResults(context);
  CHECK(PlayerOrder(lfg_state) == std::vector<std::uint64_t>{1, 3, 2});

  lfg_state.PromoteSearchSortKey(LfgSearchSortKey::kLevel);  // descending
  lfg_state.ResortSearchResults(context);
  CHECK(PlayerOrder(lfg_state) == std::vector<std::uint64_t>{2, 3, 1});

  // Zone first, then level (descending) breaks the Dalaran tie.
  lfg_state.PromoteSearchSortKey(LfgSearchSortKey::kZone);
  lfg_state.ResortSearchResults(context);
  CHECK(PlayerOrder(lfg_state) == std::vector<std::uint64_t>{2, 3, 1});

  // Names compare with the injected (case-sensitive here) comparison.
  lfg_state.PromoteSearchSortKey(LfgSearchSortKey::kName);
  player_lookups = 0;
  lfg_state.ResortSearchResults(context);
  CHECK(PlayerOrder(lfg_state) == std::vector<std::uint64_t>{3, 1, 2});
  CHECK(player_lookups == 3);  // each player looked up once per sort

  // With no player data, the name key ties and later keys decide.
  context.find_player = [](std::uint64_t) { return std::nullopt; };
  lfg_state.ResortSearchResults(context);
  CHECK(PlayerOrder(lfg_state) == std::vector<std::uint64_t>{2, 3, 1});
}
