#include "openwow/data/formats/dbc/faction_reaction.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <utility>
#include <vector>

using openwow::data::dbc::ComputeCorpseReactionLevel;
using openwow::data::dbc::ComputeFactionReaction;
using openwow::data::dbc::ComputeFactionReactionForEntries;
using openwow::data::dbc::DbcStore;
using openwow::data::dbc::DbcTableSchema;
using openwow::data::dbc::FactionTemplateEntry;
using openwow::data::dbc::IsPvPFactionTemplate;
using openwow::data::dbc::kFactionFlagHostileByDefault;
using openwow::data::dbc::kFactionFlagPvP;
using openwow::game::ReactionType;

namespace {

FactionTemplateEntry MakeEntry(std::uint32_t id, std::uint32_t faction, std::uint32_t flags = 0,
                               std::uint32_t faction_group = 0, std::uint32_t friend_group = 0,
                               std::uint32_t enemy_group = 0) {
  FactionTemplateEntry entry{};
  entry.id = id;
  entry.faction = faction;
  entry.flags = flags;
  entry.faction_group = faction_group;
  entry.friend_group = friend_group;
  entry.enemy_group = enemy_group;
  entry.enemies = {0, 0, 0, 0};
  entry.friends = {0, 0, 0, 0};
  return entry;
}

} // namespace

TEST_CASE("ComputeFactionReactionForEntries: overlapping enemy_group/faction_group is hostile",
          "[data][faction_reaction]") {
  auto a = MakeEntry(1, 100, 0, /*faction_group=*/0, /*friend_group=*/0, /*enemy_group=*/0x02);
  auto b = MakeEntry(2, 200, 0, /*faction_group=*/0x02);
  CHECK(ComputeFactionReactionForEntries(a, b) == ReactionType::kHostile);
}

TEST_CASE("ComputeFactionReactionForEntries: an explicit enemies[] entry is hostile",
          "[data][faction_reaction]") {
  auto a = MakeEntry(1, 100);
  a.enemies = {200, 0, 0, 0};
  auto b = MakeEntry(2, 200);
  CHECK(ComputeFactionReactionForEntries(a, b) == ReactionType::kHostile);
}

TEST_CASE("ComputeFactionReactionForEntries: a zero entry in enemies[]/friends[] never "
          "matches a faction id of 0",
          "[data][faction_reaction]") {
  // FLAGGED FOR FUTURE WORK (documented, not fixed): 0 is used as a
  // sentinel for "unset" relation slots (HasEnemyFaction/HasFriendFaction
  // both skip `enemy/friend_faction != 0` explicitly). This means faction
  // id 0 can never legitimately appear in anyone's enemies/friends list,
  // even if two real entries both happen to have faction == 0 -- verified
  // here rather than assumed.
  auto a = MakeEntry(1, /*faction=*/0);
  auto b = MakeEntry(2, /*faction=*/0); // both "unset"-looking factions
  CHECK(ComputeFactionReactionForEntries(a, b) == ReactionType::kNeutral);
}

TEST_CASE("ComputeFactionReactionForEntries: overlapping friend_group in either direction "
          "is friendly",
          "[data][faction_reaction]") {
  auto a = MakeEntry(1, 100, 0, /*faction_group=*/0x04, /*friend_group=*/0x01);
  auto b = MakeEntry(2, 200, 0, /*faction_group=*/0x01, /*friend_group=*/0);
  CHECK(ComputeFactionReactionForEntries(a, b) == ReactionType::kFriendly);

  // Reversed: b's friend_group overlaps a's faction_group instead.
  auto c = MakeEntry(3, 300, 0, /*faction_group=*/0x08, /*friend_group=*/0);
  auto d = MakeEntry(4, 400, 0, /*faction_group=*/0, /*friend_group=*/0x08);
  CHECK(ComputeFactionReactionForEntries(c, d) == ReactionType::kFriendly);
}

TEST_CASE("ComputeFactionReactionForEntries: an explicit friends[] entry is friendly in "
          "either direction",
          "[data][faction_reaction]") {
  auto a = MakeEntry(1, 100);
  a.friends = {200, 0, 0, 0};
  auto b = MakeEntry(2, 200);
  CHECK(ComputeFactionReactionForEntries(a, b) == ReactionType::kFriendly);

  auto c = MakeEntry(3, 300);
  auto d = MakeEntry(4, 400);
  d.friends = {300, 0, 0, 0};
  CHECK(ComputeFactionReactionForEntries(c, d) == ReactionType::kFriendly);
}

TEST_CASE("ComputeFactionReactionForEntries: hostility takes priority over friendliness "
          "when both would otherwise apply",
          "[data][faction_reaction]") {
  auto a = MakeEntry(1, 100, 0, /*faction_group=*/0x02, /*friend_group=*/0x01,
                     /*enemy_group=*/0x02);
  auto b = MakeEntry(2, 200, 0, /*faction_group=*/0x02, /*friend_group=*/0x01);
  // a.enemy_group & b.faction_group != 0 (hostile) AND a.friend_group &
  // b.faction_group != 0 (would be friendly) both hold; hostility is
  // checked first in the source and wins.
  CHECK(ComputeFactionReactionForEntries(a, b) == ReactionType::kHostile);
}

TEST_CASE("ComputeFactionReactionForEntries: kFactionFlagHostileByDefault applies only "
          "when nothing else matched",
          "[data][faction_reaction]") {
  auto a = MakeEntry(1, 100, kFactionFlagHostileByDefault);
  auto b = MakeEntry(2, 200);
  CHECK(ComputeFactionReactionForEntries(a, b) == ReactionType::kHostile);
}

TEST_CASE("ComputeFactionReactionForEntries: no relation at all is neutral",
          "[data][faction_reaction]") {
  auto a = MakeEntry(1, 100);
  auto b = MakeEntry(2, 200);
  CHECK(ComputeFactionReactionForEntries(a, b) == ReactionType::kNeutral);
}

TEST_CASE("ComputeFactionReaction: identical non-zero faction ids are friendly without "
          "needing a store lookup",
          "[data][faction_reaction]") {
  DbcStore<FactionTemplateEntry> empty_store; // never loaded -- any lookup would fail
  CHECK(ComputeFactionReaction(42, 42, empty_store) == ReactionType::kFriendly);
}

TEST_CASE("ComputeFactionReaction: identical faction id 0 is NOT short-circuited to friendly",
          "[data][faction_reaction]") {
  DbcStore<FactionTemplateEntry> empty_store;
  CHECK(ComputeFactionReaction(0, 0, empty_store) == ReactionType::kNeutral);
}

TEST_CASE("ComputeFactionReaction/ComputeCorpseReactionLevel/IsPvPFactionTemplate all "
          "fall back safely when a faction template id isn't found in the store",
          "[data][faction_reaction]") {
  DbcStore<FactionTemplateEntry> empty_store;
  CHECK(ComputeFactionReaction(1, 2, empty_store) == ReactionType::kNeutral);
  CHECK(ComputeCorpseReactionLevel(1, 2, empty_store) == static_cast<int>(ReactionType::kNeutral));
  CHECK_FALSE(IsPvPFactionTemplate(1, empty_store));
}

namespace {

DbcTableSchema<FactionTemplateEntry> TestFactionTemplateSchema() {
  return DbcTableSchema<FactionTemplateEntry>{
      .retail_path = "Test\\FactionTemplate.dbc",
      .field_count = 14, // id, faction, flags, faction_group, friend_group, enemy_group, 4x
                         // enemies, 4x friends
      .record_size = 56, // 14 * sizeof(uint32_t)
      .decode = &FactionTemplateEntry::Load,
  };
}

void AppendLe32(std::vector<std::uint8_t> &out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

void AppendFactionTemplateRecord(std::vector<std::uint8_t> &out, std::uint32_t id,
                                 std::uint32_t faction, std::uint32_t flags,
                                 std::uint32_t faction_group, std::uint32_t friend_group,
                                 std::uint32_t enemy_group) {
  AppendLe32(out, id);
  AppendLe32(out, faction);
  AppendLe32(out, flags);
  AppendLe32(out, faction_group);
  AppendLe32(out, friend_group);
  AppendLe32(out, enemy_group);
  for (int i = 0; i < 4; ++i)
    AppendLe32(out, 0u); // enemies[]
  for (int i = 0; i < 4; ++i)
    AppendLe32(out, 0u); // friends[]
}

openwow::data::dbc::DbcFile
BuildFactionTemplateDbcFile(const std::vector<std::array<std::uint32_t, 6>> &rows) {
  std::vector<std::uint8_t> data;
  AppendLe32(data, 0x43424457u); // "WDBC"
  AppendLe32(data, static_cast<std::uint32_t>(rows.size()));
  AppendLe32(data, 14u); // field_count
  AppendLe32(data, 56u); // record_size
  AppendLe32(data, 0u);  // string_block_size
  for (const auto &row : rows) {
    AppendFactionTemplateRecord(data, row[0], row[1], row[2], row[3], row[4], row[5]);
  }
  openwow::data::dbc::DbcFile file;
  file.LoadFromBytes(data);
  return file;
}

} // namespace

TEST_CASE("ComputeFactionReaction/IsPvPFactionTemplate resolve real entries loaded through "
          "DbcStore",
          "[data][faction_reaction]") {
  // faction_template 1: faction_group 0x01, enemy_group 0x02 (hostile to
  // template 2). faction_template 2: faction_group 0x02, PvP flag set.
  //
  // ComputeFactionReaction(a, b, store) checks a's enemy_group against b's
  // faction_group (i.e. the FIRST id's enemy_group, not the second's) --
  // an earlier version of this test put enemy_group on template 2 instead
  // of template 1 and got kNeutral back rather than kHostile, since
  // nothing here ever set template 1's enemy_group. Fixed by putting the
  // enemy_group on the correct (first) entry.
  auto file = BuildFactionTemplateDbcFile({
      {1, 100, 0, 0x01, 0, 0x02},
      {2, 200, kFactionFlagPvP, 0x02, 0, 0},
  });
  REQUIRE(file.loaded());

  DbcStore<FactionTemplateEntry> store;
  const auto result = store.LoadFromFile(std::move(file), TestFactionTemplateSchema());
  REQUIRE(result.succeeded());
  REQUIRE(store.size() == 2);

  CHECK(ComputeFactionReaction(1, 2, store) == ReactionType::kHostile);
  CHECK_FALSE(IsPvPFactionTemplate(1, store));
  CHECK(IsPvPFactionTemplate(2, store));
}
