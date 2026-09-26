#include "openwow/game/social/contacts/adapters/protocol/contact_server_packets.h"

#include "payload_builder.h"

#include <catch2/catch_test_macros.hpp>

namespace contacts = openwow::game::contacts;
using Bytes = openwow::test::PayloadBuilder;

namespace {

template <typename Decode>
void CheckEveryPrefixFails(const Bytes& bytes, Decode decode) {
  for (std::size_t cut = 1; cut <= bytes.size(); ++cut) {
    INFO("cut " << cut << " of " << bytes.size());
    CHECK_FALSE(decode(bytes.truncated(cut)).has_value());
  }
}

}  // namespace

TEST_CASE("Contact list decoding", "[contacts][protocol]") {
  const Bytes bytes = Bytes()
                          .u32(0x7).u32(3)
                          .u64(0xA).u32(0x1).str("tank").u8(1).u32(1519).u32(80).u32(1)  // online friend
                          .u64(0xB).u32(0x1).str("").u8(0)                               // offline friend
                          .u64(0xC).u32(0x2 | 0x4).str("");                              // ignored + muted
  const auto m = contacts::DecodeContactList(bytes.span());
  REQUIRE(m);
  CHECK(m->flags == 0x7u);
  REQUIRE(m->entries.size() == 3u);
  CHECK(m->entries[0].guid == 0xAu);
  CHECK(m->entries[0].note == "tank");
  CHECK(m->entries[0].presence.status == 1u);
  CHECK(m->entries[0].presence.area == 1519u);
  CHECK(m->entries[0].presence.level == 80u);
  CHECK(m->entries[0].presence.player_class == 1u);
  CHECK(m->entries[1].presence.status == 0u);
  CHECK(m->entries[1].presence.area == 0u);
  CHECK(m->entries[2].flags == 0x6u);
  CheckEveryPrefixFails(bytes, contacts::DecodeContactList);
}

TEST_CASE("Contact list rejects counts the payload can't hold", "[contacts][protocol]") {
  // Two entries claimed, room for at most one 13-byte entry.
  const Bytes bytes = Bytes().u32(1).u32(2).u64(0xA).u32(0x2).str("");
  CHECK_FALSE(contacts::DecodeContactList(bytes.span()));
  const auto empty = contacts::DecodeContactList(Bytes().u32(1).u32(0).span());
  REQUIRE(empty);
  CHECK(empty->entries.empty());
}

TEST_CASE("Friend status decoding by result", "[contacts][protocol]") {
  SECTION("online") {
    const Bytes b = Bytes().u8(0x02).u64(0xA).u8(1).u32(12).u32(70).u32(8);
    const auto m = contacts::DecodeFriendStatus(b.span());
    REQUIRE(m);
    CHECK(m->result == 0x02u);
    CHECK(m->presence.area == 12u);
    CHECK(m->presence.player_class == 8u);
    CheckEveryPrefixFails(b, contacts::DecodeFriendStatus);
  }
  SECTION("added online carries note and presence") {
    const Bytes b = Bytes().u8(0x06).u64(0xA).str("pal").u8(1).u32(12).u32(70).u32(2);
    const auto m = contacts::DecodeFriendStatus(b.span());
    REQUIRE(m);
    CHECK(m->note == "pal");
    CHECK(m->presence.level == 70u);
    CheckEveryPrefixFails(b, contacts::DecodeFriendStatus);
  }
  SECTION("added offline carries only the note") {
    const Bytes b = Bytes().u8(0x07).u64(0xA).str("pal");
    const auto m = contacts::DecodeFriendStatus(b.span());
    REQUIRE(m);
    CHECK(m->note == "pal");
    CHECK(m->presence.status == 0u);
    CheckEveryPrefixFails(b, contacts::DecodeFriendStatus);
  }
  SECTION("status and area changes") {
    const auto afk = contacts::DecodeFriendStatus(Bytes().u8(0x1A).u64(0xA).u8(2).span());
    REQUIRE(afk);
    CHECK(afk->presence.status == 2u);
    const auto moved = contacts::DecodeFriendStatus(Bytes().u8(0x1B).u64(0xA).u32(4395).span());
    REQUIRE(moved);
    CHECK(moved->presence.area == 4395u);
    CHECK_FALSE(contacts::DecodeFriendStatus(Bytes().u8(0x1B).u64(0xA).u8(1).span()));
  }
  SECTION("results without extra data") {
    const Bytes b = Bytes().u8(0x05).u64(0xA);
    const auto m = contacts::DecodeFriendStatus(b.span());
    REQUIRE(m);
    CHECK(m->guid == 0xAu);
    CheckEveryPrefixFails(b, contacts::DecodeFriendStatus);
  }
}
