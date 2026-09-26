#include "openwow/game/activities/lfg/adapters/protocol/lfg_server_packets.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace lfg = openwow::game::lfg;
using openwow::game::LfgUpdateType;

namespace {

// Little-endian payload builder.
class Bytes {
 public:
  Bytes& u8(std::uint8_t v) { data_.push_back(v); return *this; }
  Bytes& u32(std::uint32_t v) { return raw(&v, 4); }
  Bytes& i32(std::int32_t v) { return raw(&v, 4); }
  Bytes& u64(std::uint64_t v) { return raw(&v, 8); }
  Bytes& str(std::string_view s) {
    data_.insert(data_.end(), s.begin(), s.end());
    data_.push_back(0);
    return *this;
  }
  [[nodiscard]] lfg::Payload span() const { return data_; }
  // The payload with its last `n` bytes cut off.
  [[nodiscard]] lfg::Payload truncated(std::size_t n = 1) const {
    return lfg::Payload(data_).first(data_.size() - n);
  }
  [[nodiscard]] std::size_t size() const { return data_.size(); }

 private:
  Bytes& raw(const void* p, std::size_t n) {
    const auto* b = static_cast<const std::uint8_t*>(p);
    data_.insert(data_.end(), b, b + n);
    return *this;
  }
  std::vector<std::uint8_t> data_;
};

// Every strict prefix of `bytes` must fail to decode.
template <typename Decode>
void CheckEveryPrefixFails(const Bytes& bytes, Decode decode) {
  for (std::size_t cut = 1; cut <= bytes.size(); ++cut) {
    INFO("cut " << cut << " of " << bytes.size());
    CHECK_FALSE(decode(bytes.truncated(cut)).has_value());
  }
}

}  // namespace

TEST_CASE("LFG join result without locks", "[lfg][protocol]") {
  const Bytes bytes = Bytes().u32(0).u32(4);
  const auto m = lfg::DecodeJoinResult(bytes.span());
  REQUIRE(m);
  CHECK(m->result.result == 0u);
  CHECK(m->result.state == 4u);
  CHECK_FALSE(m->party_locks);
  CheckEveryPrefixFails(bytes, lfg::DecodeJoinResult);
}

TEST_CASE("LFG join result 6 carries party locks", "[lfg][protocol]") {
  const Bytes bytes = Bytes()
                          .u32(6).u32(0)
                          .u8(2)                                       // members
                          .u64(0xA).u32(2).u32(0x01000010).u32(1)      // member A
                          .u32(0x01000010).u32(3)                      // same dungeon again
                          .u64(0xB).u32(1).u32(0x02000020).u32(4);     // member B
  const auto m = lfg::DecodeJoinResult(bytes.span());
  REQUIRE(m);
  REQUIRE(m->party_locks);
  REQUIRE(m->party_locks->size() == 2u);
  CHECK((*m->party_locks)[0].guid == 0xAu);
  REQUIRE((*m->party_locks)[0].locks.size() == 1u);  // later reason wins
  CHECK((*m->party_locks)[0].locks[0].lock_status == 3u);
  REQUIRE(m->result.player_locks.size() == 2u);
  CHECK(m->result.player_locks[1].player_guid == 0xBu);
  CHECK(m->result.player_locks[1].locks[0].dungeon_entry == 0x02000020u);
  CheckEveryPrefixFails(bytes, lfg::DecodeJoinResult);
  // Result 6 needs at least the member count byte.
  CHECK_FALSE(lfg::DecodeJoinResult(Bytes().u32(6).u32(0).span()));
}

TEST_CASE("LFG queue status", "[lfg][protocol]") {
  const Bytes bytes = Bytes().u32(0x06000105).i32(120).i32(-1).i32(30).i32(60).i32(5)
                          .u8(1).u8(0).u8(2).u32(95);
  const auto qs = lfg::DecodeQueueStatus(bytes.span());
  REQUIRE(qs);
  CHECK(qs->dungeon_id == 0x06000105u);
  CHECK(qs->wait_time_avg == 120);
  CHECK(qs->wait_time == -1);
  CHECK(qs->wait_time_dps == 5);
  CHECK(qs->tanks_needed == 1u);
  CHECK(qs->dps_needed == 2u);
  CHECK(qs->queued_time == 95u);
  CheckEveryPrefixFails(bytes, lfg::DecodeQueueStatus);
}

TEST_CASE("LFG player update", "[lfg][protocol]") {
  SECTION("without extra data") {
    const auto u = lfg::DecodeUpdatePlayer(Bytes().u8(6).u8(0).span());
    REQUIRE(u);
    CHECK(u->update_type == LfgUpdateType::kRemovedFromQueue);
    CHECK_FALSE(u->has_extra);
    CHECK_FALSE(u->joined);
    CHECK(u->dungeons.empty());
  }
  SECTION("with extra data; duplicate dungeons collapse") {
    const Bytes bytes = Bytes().u8(13).u8(1).u8(1).u8(0).u8(1)
                            .u8(3).u32(0x01000010).u32(0x05000011).u32(0x01000010)
                            .str("hello");
    const auto u = lfg::DecodeUpdatePlayer(bytes.span());
    REQUIRE(u);
    CHECK(u->update_type == LfgUpdateType::kUpdateStatus);
    CHECK(u->joined);  // mirrors has_extra for the player
    CHECK(u->queued);
    CHECK_FALSE(u->raw_flag_4);
    CHECK(u->raw_flag_5);
    CHECK(u->dungeons == std::vector<std::uint32_t>{0x01000010u, 0x05000011u});
    CHECK(u->comment == "hello");
    CheckEveryPrefixFails(bytes, lfg::DecodeUpdatePlayer);
  }
}

TEST_CASE("LFG party update", "[lfg][protocol]") {
  const Bytes bytes = Bytes().u8(4).u8(1).u8(0).u8(1).u8(1).u8(0)
                          .u8(7).u8(8).u8(9)
                          .u8(1).u32(0x02000020)
                          .str("");
  const auto u = lfg::DecodeUpdateParty(bytes.span());
  REQUIRE(u);
  CHECK(u->update_type == LfgUpdateType::kJoinQueue);
  CHECK_FALSE(u->joined);  // read from the wire for parties
  CHECK(u->queued);
  CHECK(u->raw_flag_4);
  CHECK(u->raw_tail_bytes == std::array<std::uint8_t, 3>{7, 8, 9});
  CHECK(u->dungeons == std::vector<std::uint32_t>{0x02000020u});
  CHECK(u->comment.empty());
  CheckEveryPrefixFails(bytes, lfg::DecodeUpdateParty);
}

TEST_CASE("LFG proposal update", "[lfg][protocol]") {
  Bytes bytes;
  bytes.u32(0x06000105).u8(2).u32(77).u32(0x3).u8(0).u8(2);
  bytes.u32(2).u8(1).u8(0).u8(1).u8(1).u8(1);
  bytes.u32(8).u8(0).u8(1).u8(0).u8(0).u8(0);
  const auto p = lfg::DecodeProposalUpdate(bytes.span());
  REQUIRE(p);
  CHECK(p->dungeon_entry == 0x06000105u);
  CHECK(p->state == 2u);
  CHECK(p->proposal_id == 77u);
  CHECK(p->encounter_mask == 3u);
  CHECK_FALSE(p->silent);
  REQUIRE(p->players.size() == 2u);
  CHECK(p->players[0].role == 2u);
  CHECK(p->players[0].is_current_player);
  CHECK(p->players[0].has_accepted);
  CHECK(p->players[1].in_dungeon);
  CHECK_FALSE(p->players[1].has_answered);
  CheckEveryPrefixFails(bytes, lfg::DecodeProposalUpdate);
}

TEST_CASE("LFG proposal update reads at most five players", "[lfg][protocol]") {
  Bytes bytes;
  bytes.u32(1).u8(0).u32(1).u32(0).u8(1).u8(9);
  for (int i = 0; i < 5; ++i) {
    bytes.u32(8).u8(0).u8(0).u8(0).u8(0).u8(0);
  }
  const auto p = lfg::DecodeProposalUpdate(bytes.span());
  REQUIRE(p);
  CHECK(p->silent);
  CHECK(p->players.size() == 5u);
}

TEST_CASE("LFG role check update", "[lfg][protocol]") {
  const Bytes bytes = Bytes().u32(1).u8(1).u8(1).u32(0x01000010)
                          .u8(1).u64(0xC).u8(1).u32(0x0A).u8(80);
  const auto rc = lfg::DecodeRoleCheckUpdate(bytes.span());
  REQUIRE(rc);
  CHECK(rc->state == 1u);
  CHECK(rc->is_beginning);
  CHECK(rc->dungeons == std::vector<std::uint32_t>{0x01000010u});
  REQUIRE(rc->players.size() == 1u);
  CHECK(rc->players[0].guid == 0xCu);
  CHECK(rc->players[0].ready);
  CHECK(rc->players[0].roles == 0x0Au);
  CHECK(rc->players[0].level == 80u);
  CheckEveryPrefixFails(bytes, lfg::DecodeRoleCheckUpdate);
}

TEST_CASE("LFG boot proposal decodes permissively", "[lfg][protocol]") {
  const Bytes full = Bytes().u8(1).u8(1).u8(0).u64(0xD).u32(4).u32(3).u32(55).u32(3)
                         .str("afk");
  const auto bp = lfg::DecodeBootProposalUpdate(full.span());
  CHECK(bp.in_progress);
  CHECK(bp.did_vote);
  CHECK_FALSE(bp.agree);
  CHECK(bp.victim_guid == 0xDu);
  CHECK(bp.total_votes == 4u);
  CHECK(bp.agree_count == 3u);
  CHECK(bp.time_left == 55u);
  CHECK(bp.needed_votes == 3u);
  CHECK(bp.reason == "afk");

  SECTION("a short payload zeroes the missing fields") {
    const auto short_bp = lfg::DecodeBootProposalUpdate(Bytes().u8(1).u8(0).u8(1).u32(7).span());
    CHECK(short_bp.in_progress);
    CHECK(short_bp.agree);
    // The guid needs 8 bytes; only 4 remain, so it and everything after read zero.
    CHECK(short_bp.victim_guid == 0u);
    CHECK(short_bp.total_votes == 0u);
    CHECK(short_bp.reason.empty());
  }
  SECTION("an unterminated reason keeps what was read") {
    Bytes b = Bytes().u8(0).u8(0).u8(0).u64(0).u32(0).u32(0).u32(0).u32(0);
    b.u8('a').u8('b');
    CHECK(lfg::DecodeBootProposalUpdate(b.span()).reason == "ab");
  }
  SECTION("the reason is capped at 256 characters") {
    Bytes b = Bytes().u8(0).u8(0).u8(0).u64(0).u32(0).u32(0).u32(0).u32(0);
    for (int i = 0; i < 300; ++i) b.u8('x');
    CHECK(lfg::DecodeBootProposalUpdate(b.span()).reason.size() == 256u);
  }
  SECTION("empty payload") {
    const auto empty = lfg::DecodeBootProposalUpdate({});
    CHECK_FALSE(empty.in_progress);
    CHECK(empty.reason.empty());
  }
}

TEST_CASE("LFG player reward", "[lfg][protocol]") {
  const Bytes bytes = Bytes().u32(0x06000105).u32(0x01000010).u8(1).u32(2).u32(1000)
                          .u32(500).u32(10).u32(20)
                          .u8(1).u32(49426).u32(1234).u32(2);
  const auto r = lfg::DecodePlayerReward(bytes.span());
  REQUIRE(r);
  CHECK(r->random_dungeon_entry == 0x06000105u);
  CHECK(r->is_first_reward);
  CHECK(r->strangers_count == 2u);
  CHECK(r->base_money_reward == 1000u);
  CHECK(r->variable_xp_reward == 20u);
  REQUIRE(r->items.size() == 1u);
  CHECK(r->items[0].item_id == 49426u);
  CHECK(r->items[0].item_count == 2u);
  CheckEveryPrefixFails(bytes, lfg::DecodePlayerReward);
}

TEST_CASE("LFG single-value packets", "[lfg][protocol]") {
  const Bytes bytes = Bytes().u32(0x01020304);
  CHECK(lfg::DecodeTeleportDenied(bytes.span()) == 0x01020304u);
  CHECK(lfg::DecodeOfferContinue(bytes.span()) == 0x01020304u);
  CHECK(lfg::DecodeOpenDungeonFinder(bytes.span()) == 0x01020304u);
  CheckEveryPrefixFails(bytes, lfg::DecodeTeleportDenied);
  CheckEveryPrefixFails(bytes, lfg::DecodeOfferContinue);
  CheckEveryPrefixFails(bytes, lfg::DecodeOpenDungeonFinder);

  CHECK(lfg::DecodeUpdateSearch(Bytes().u8(1).span()) == std::uint8_t{1});
  CHECK_FALSE(lfg::DecodeUpdateSearch({}));

  const Bytes chosen = Bytes().u64(0xE).u8(1).u32(0x08);
  const auto rc = lfg::DecodeRoleChosen(chosen.span());
  REQUIRE(rc);
  CHECK(rc->guid == 0xEu);
  CHECK(rc->ready == 1u);
  CHECK(rc->roles == 0x08u);
  CheckEveryPrefixFails(chosen, lfg::DecodeRoleChosen);
}

TEST_CASE("LFG player info merges repeated dungeons", "[lfg][protocol]") {
  const Bytes bytes = Bytes()
                          .u8(2)
                          .u32(0x06000105).u8(1).u32(100).u32(200).u32(3).u32(4)
                          .u8(1).u32(1).u32(2).u32(3)
                          .u32(0x06000105).u8(0).u32(101).u32(201).u32(5).u32(6)
                          .u8(1).u32(7).u32(8).u32(9)
                          .u32(2)
                          .u32(0x06000105).u32(1004)
                          .u32(0x01000010).u32(1001);
  const auto states = lfg::DecodePlayerInfo(bytes.span());
  REQUIRE(states);
  REQUIRE(states->size() == 2u);
  const auto& random = (*states)[0];
  CHECK(random.packed_dungeon_id == 0x06000105u);
  CHECK_FALSE(random.reward_done);   // later values win
  CHECK(random.reward_money == 101u);
  CHECK(random.rewards.size() == 2u);  // rewards accumulate
  CHECK(random.locked);
  CHECK(random.lock_reason == 1004u);
  const auto& locked_only = (*states)[1];
  CHECK(locked_only.packed_dungeon_id == 0x01000010u);
  CHECK(locked_only.locked);
  CHECK(locked_only.rewards.empty());
  CheckEveryPrefixFails(bytes, lfg::DecodePlayerInfo);
}

TEST_CASE("LFG party info", "[lfg][protocol]") {
  const Bytes bytes = Bytes().u8(1).u64(0xA).u32(1).u32(0x01000010).u32(2);
  const auto locks = lfg::DecodePartyInfo(bytes.span());
  REQUIRE(locks);
  REQUIRE(locks->size() == 1u);
  CHECK((*locks)[0].guid == 0xAu);
  CHECK((*locks)[0].locks[0].lock_status == 2u);
  CheckEveryPrefixFails(bytes, lfg::DecodePartyInfo);
  const auto none = lfg::DecodePartyInfo(Bytes().u8(0).span());
  REQUIRE(none);
  CHECK(none->empty());
}

namespace {

Bytes FullSearchList() {
  Bytes b;
  b.u32(2).u32(0x0000A1);                    // type 2 (raid), dungeon 0xA1
  b.u8(1).u32(1).u64(0x99);                  // delete list: one guid
  b.u32(1).u32(10);                          // one group of 10 reported
  b.u64(0x100).u32(0x2 | 0x80).str("LF1M").u64(0x200).u32(0x7);
  b.u32(1).u32(20);                          // one player of 20 reported
  b.u64(0x300).u32(0x4 | 0x8 | 0x20).u8(1).u64(0x100).u32(1519);
  return b;
}

}  // namespace

TEST_CASE("LFG search list update decodes every section", "[lfg][protocol]") {
  const auto u = lfg::DecodeSearchListUpdate(FullSearchList().span());
  CHECK(u.stage == lfg::SearchListStage::kComplete);
  CHECK(u.packed_search_id == 0x020000A1u);
  CHECK_FALSE(u.replaces_all);
  CHECK(u.deleted_guids == std::vector<std::uint64_t>{0x99u});
  CHECK(u.reported_group_total == 10u);
  REQUIRE(u.groups.size() == 1u);
  CHECK(u.groups[0].guid == 0x100u);
  CHECK(u.groups[0].mask == 0x82u);
  CHECK(u.groups[0].fields.comment == "LF1M");
  CHECK(u.groups[0].fields.encounter_guid == 0x200u);
  CHECK(u.groups[0].fields.encounter_mask == 7u);
  CHECK(u.reported_player_total == 20u);
  REQUIRE(u.players.size() == 1u);
  CHECK(u.players[0].guid == 0x300u);
  CHECK(u.players[0].fields.joined_group);
  CHECK(u.players[0].fields.resolved_group_guid == 0x100u);
  CHECK(u.players[0].fields.area_id == 1519u);
}

TEST_CASE("LFG search list update without a delete list replaces everything",
          "[lfg][protocol]") {
  const auto u = lfg::DecodeSearchListUpdate(
      Bytes().u32(1).u32(5).u8(0).u32(0).u32(0).u32(0).u32(0).span());
  CHECK(u.stage == lfg::SearchListStage::kComplete);
  CHECK(u.replaces_all);
  CHECK(u.deleted_guids.empty());
}

TEST_CASE("LFG search list update reports how far a short payload got",
          "[lfg][protocol]") {
  using Stage = lfg::SearchListStage;
  const Bytes full = FullSearchList();
  const auto stage_at = [&](std::size_t length) {
    return lfg::DecodeSearchListUpdate(full.span().first(length));
  };
  CHECK(stage_at(0).stage == Stage::kNothing);
  CHECK(stage_at(7).stage == Stage::kNothing);
  CHECK(stage_at(8).stage == Stage::kSearchId);
  CHECK(stage_at(9).stage == Stage::kDeleteFlag);
  CHECK(stage_at(20).stage == Stage::kDeleteFlag);  // delete guid cut short
  CHECK(stage_at(21).stage == Stage::kDeletes);
  CHECK(stage_at(29).stage == Stage::kGroupHeader);

  // Cut inside the group entry: the header counts, the entry doesn't.
  const auto mid_group = stage_at(40);
  CHECK(mid_group.stage == Stage::kGroupHeader);
  CHECK(mid_group.reported_group_total == 10u);
  CHECK(mid_group.groups.empty());

  const std::size_t groups_end = 29 + 8 + 4 + 5 + 8 + 4;
  CHECK(stage_at(groups_end).stage == Stage::kGroups);
  CHECK(stage_at(groups_end).groups.size() == 1u);
  CHECK(stage_at(groups_end + 8).stage == Stage::kPlayerHeader);
  const auto mid_player = stage_at(full.size() - 1);
  CHECK(mid_player.stage == Stage::kPlayerHeader);
  CHECK(mid_player.players.empty());
}

TEST_CASE("LFG search deltas copy only the fields their mask carries",
          "[lfg][protocol]") {
  openwow::game::LfgSearchGroupResult group;
  group.comment = "old";
  group.encounter_mask = 1u;
  group.raw_u8_292_294 = {1, 2, 3};

  lfg::SearchGroupDelta group_delta;
  group_delta.mask = 0x2u;
  group_delta.fields.comment = "new";
  group_delta.fields.encounter_mask = 99u;
  lfg::ApplySearchGroupDelta(group_delta, group);
  CHECK(group.comment == "new");
  CHECK(group.encounter_mask == 1u);
  CHECK(group.raw_u8_292_294 == std::array<std::uint8_t, 3>{1, 2, 3});

  group_delta.mask = 0x10u | 0x80u;
  group_delta.fields.raw_u8_292_294 = {7, 8, 9};
  lfg::ApplySearchGroupDelta(group_delta, group);
  CHECK(group.encounter_mask == 99u);
  CHECK(group.raw_u8_292_294 == std::array<std::uint8_t, 3>{7, 8, 9});
  CHECK(group.comment == "new");

  openwow::game::LfgSearchPlayerResult player;
  player.level = 70u;
  player.area_id = 12u;
  player.role_byte = 4u;
  lfg::SearchPlayerDelta player_delta;
  player_delta.mask = 0x20u;
  player_delta.fields.area_id = 1519u;
  player_delta.fields.level = 80u;
  lfg::ApplySearchPlayerDelta(player_delta, player);
  CHECK(player.area_id == 1519u);
  CHECK(player.level == 70u);
  CHECK(player.role_byte == 4u);

  player_delta.mask = 0x1u | 0x40u;
  player_delta.fields.role_byte = 8u;
  lfg::ApplySearchPlayerDelta(player_delta, player);
  CHECK(player.level == 80u);
  CHECK(player.role_byte == 8u);
}
