#include "openwow/game/social/contacts/adapters/protocol/contact_client_packets.h"

#include "openwow/network/protocol/wotlk/opcodes.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace contacts = openwow::game::contacts;
using openwow::net::wotlk::Opcode;
using Bytes = std::vector<std::uint8_t>;

namespace {
const Bytes kGuidBytes{0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
constexpr std::uint64_t kGuid = 0x0102030405060708ull;
}  // namespace

TEST_CASE("Contact list request", "[contacts][protocol]") {
  const auto pkt = contacts::BuildContactListRequestPacket(0x7u);
  CHECK(pkt.GetOpcode() == Opcode::CMSG_CONTACT_LIST);
  CHECK(pkt.payload == Bytes{0x07, 0x00, 0x00, 0x00});
}

TEST_CASE("Friend packets", "[contacts][protocol]") {
  const auto add = contacts::BuildAddFriendPacket("Bob", "tank");
  CHECK(add.GetOpcode() == Opcode::CMSG_ADD_FRIEND);
  CHECK(add.payload == Bytes{'B', 'o', 'b', 0, 't', 'a', 'n', 'k', 0});
  CHECK(contacts::BuildAddFriendPacket("Bob").payload == Bytes{'B', 'o', 'b', 0, 0});

  const auto del = contacts::BuildDelFriendPacket(kGuid);
  CHECK(del.GetOpcode() == Opcode::CMSG_DEL_FRIEND);
  CHECK(del.payload == kGuidBytes);

  const auto notes = contacts::BuildSetContactNotesPacket(kGuid, "hi");
  CHECK(notes.GetOpcode() == Opcode::CMSG_SET_CONTACT_NOTES);
  Bytes expected = kGuidBytes;
  expected.insert(expected.end(), {'h', 'i', 0});
  CHECK(notes.payload == expected);
}

TEST_CASE("Ignore and mute packets", "[contacts][protocol]") {
  CHECK(contacts::BuildAddIgnorePacket("Eve").GetOpcode() == Opcode::CMSG_ADD_IGNORE);
  CHECK(contacts::BuildAddIgnorePacket("Eve").payload == Bytes{'E', 'v', 'e', 0});
  CHECK(contacts::BuildDelIgnorePacket(kGuid).GetOpcode() == Opcode::CMSG_DEL_IGNORE);
  CHECK(contacts::BuildDelIgnorePacket(kGuid).payload == kGuidBytes);
  CHECK(contacts::BuildAddMutePacket("Eve").GetOpcode() == Opcode::CMSG_ADD_VOICE_IGNORE);
  CHECK(contacts::BuildAddMutePacket("").payload == Bytes{0});
  CHECK(contacts::BuildDelMutePacket(kGuid).GetOpcode() == Opcode::CMSG_DEL_VOICE_IGNORE);
  CHECK(contacts::BuildDelMutePacket(kGuid).payload == kGuidBytes);
}

TEST_CASE("Contact names and notes end at the first NUL", "[contacts][protocol]") {
  using namespace std::string_view_literals;
  CHECK(contacts::BuildAddIgnorePacket("Ev\0e"sv).payload == Bytes{'E', 'v', 0});
  CHECK(contacts::BuildAddFriendPacket("A\0b"sv, "n\0x"sv).payload == Bytes{'A', 0, 'n', 0});
}
