#include "openwow/game/activities/lfg/adapters/protocol/lfg_client_packets.h"

#include "openwow/network/protocol/wotlk/opcodes.h"

#include <algorithm>

namespace openwow::game::lfg {

using net::wotlk::Opcode;
using net::wotlk::WorldPacket;

WorldPacket BuildJoinPacket(const std::uint32_t roles,
                            const std::vector<std::uint32_t>& packed_dungeon_ids,
                            const std::string_view comment) {
  WorldPacket pkt(Opcode::CMSG_LFG_JOIN);
  pkt.AppendU32(roles);
  pkt.AppendU8(0);  // no partial
  pkt.AppendU8(0);  // achievements
  const auto count = static_cast<std::uint8_t>(
      std::min<std::size_t>(packed_dungeon_ids.size(), 255u));
  pkt.AppendU8(count);
  for (std::uint8_t i = 0; i < count; ++i) {
    pkt.AppendU32(packed_dungeon_ids[i]);
  }
  // Needs block: a count of 3 followed by three zero bytes.
  pkt.AppendU8(3);
  pkt.AppendU8(0);
  pkt.AppendU8(0);
  pkt.AppendU8(0);
  pkt.AppendString(comment);
  return pkt;
}

WorldPacket BuildLeavePacket() { return WorldPacket(Opcode::CMSG_LFG_LEAVE); }

WorldPacket BuildProposalResultPacket(const std::uint32_t proposal_id, const bool accept) {
  WorldPacket pkt(Opcode::CMSG_LFG_PROPOSAL_RESULT);
  pkt.AppendU32(proposal_id);
  pkt.AppendU8(accept ? 1 : 0);
  return pkt;
}

WorldPacket BuildSetRolesPacket(const std::uint8_t roles) {
  WorldPacket pkt(Opcode::CMSG_LFG_SET_ROLES);
  pkt.AppendU8(roles);
  return pkt;
}

WorldPacket BuildSetCommentPacket(const std::string_view comment) {
  WorldPacket pkt(Opcode::CMSG_SET_LFG_COMMENT);
  pkt.AppendString(comment.substr(0, comment.find('\0')));
  return pkt;
}

WorldPacket BuildSetBootVotePacket(const bool agree) {
  WorldPacket pkt(Opcode::CMSG_LFG_SET_BOOT_VOTE);
  pkt.AppendU8(agree ? 1 : 0);
  return pkt;
}

WorldPacket BuildTeleportPacket(const bool out) {
  WorldPacket pkt(Opcode::CMSG_LFG_TELEPORT);
  pkt.AppendU8(out ? 1 : 0);
  return pkt;
}

}  // namespace openwow::game::lfg
