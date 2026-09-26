#include "openwow/game/activities/lfg/adapters/protocol/lfg_client_packets.h"

#include "openwow/network/protocol/wotlk/opcodes.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace lfg = openwow::game::lfg;
using openwow::net::wotlk::Opcode;
using Bytes = std::vector<std::uint8_t>;

TEST_CASE("LFG join packet layout", "[lfg][protocol]") {
  const auto pkt = lfg::BuildJoinPacket(0x0Au, {0x06000105u, 0x01000010u}, "hi");
  CHECK(pkt.GetOpcode() == Opcode::CMSG_LFG_JOIN);
  CHECK(pkt.payload == Bytes{
                           0x0A, 0x00, 0x00, 0x00,  // roles
                           0x00, 0x00,              // no partial, achievements
                           0x02,                    // dungeon count
                           0x05, 0x01, 0x00, 0x06,  // dungeons
                           0x10, 0x00, 0x00, 0x01,
                           0x03, 0x00, 0x00, 0x00,  // needs block
                           'h', 'i', 0x00,          // comment
                       });
}

TEST_CASE("LFG join packet sends at most 255 dungeons", "[lfg][protocol]") {
  const std::vector<std::uint32_t> dungeons(300u, 0x01000001u);
  const auto pkt = lfg::BuildJoinPacket(0u, dungeons, "");
  REQUIRE(pkt.payload.size() == 4u + 2u + 1u + 255u * 4u + 4u + 1u);
  CHECK(pkt.payload[6] == 255u);
}

TEST_CASE("LFG small client packets", "[lfg][protocol]") {
  const auto leave = lfg::BuildLeavePacket();
  CHECK(leave.GetOpcode() == Opcode::CMSG_LFG_LEAVE);
  CHECK(leave.payload.empty());

  const auto proposal = lfg::BuildProposalResultPacket(0x01020304u, true);
  CHECK(proposal.GetOpcode() == Opcode::CMSG_LFG_PROPOSAL_RESULT);
  CHECK(proposal.payload == Bytes{0x04, 0x03, 0x02, 0x01, 0x01});
  CHECK(lfg::BuildProposalResultPacket(1u, false).payload == Bytes{0x01, 0x00, 0x00, 0x00, 0x00});

  const auto roles = lfg::BuildSetRolesPacket(0x0Cu);
  CHECK(roles.GetOpcode() == Opcode::CMSG_LFG_SET_ROLES);
  CHECK(roles.payload == Bytes{0x0C});

  const auto vote = lfg::BuildSetBootVotePacket(true);
  CHECK(vote.GetOpcode() == Opcode::CMSG_LFG_SET_BOOT_VOTE);
  CHECK(vote.payload == Bytes{0x01});

  const auto teleport = lfg::BuildTeleportPacket(false);
  CHECK(teleport.GetOpcode() == Opcode::CMSG_LFG_TELEPORT);
  CHECK(teleport.payload == Bytes{0x00});
  CHECK(lfg::BuildTeleportPacket(true).payload == Bytes{0x01});
}

TEST_CASE("LFG comment packet stops at the first NUL", "[lfg][protocol]") {
  const auto pkt = lfg::BuildSetCommentPacket("ab");
  CHECK(pkt.GetOpcode() == Opcode::CMSG_SET_LFG_COMMENT);
  CHECK(pkt.payload == Bytes{'a', 'b', 0x00});

  const std::string with_nul("x\0y", 3);
  CHECK(lfg::BuildSetCommentPacket(with_nul).payload == Bytes{'x', 0x00});
  CHECK(lfg::BuildSetCommentPacket("").payload == Bytes{0x00});
}
