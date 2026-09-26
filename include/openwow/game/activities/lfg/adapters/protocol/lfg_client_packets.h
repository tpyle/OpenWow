#pragma once

#include "openwow/network/protocol/wotlk/world_packet.h"

#include <cstdint>
#include <string_view>
#include <vector>

/// Builders for the Dungeon Finder packets the client sends.
namespace openwow::game::lfg {

/// CMSG_LFG_JOIN. At most 255 dungeons are sent; the rest are dropped.
[[nodiscard]] net::wotlk::WorldPacket BuildJoinPacket(
    std::uint32_t roles, const std::vector<std::uint32_t>& packed_dungeon_ids,
    std::string_view comment);

/// CMSG_LFG_LEAVE (no payload).
[[nodiscard]] net::wotlk::WorldPacket BuildLeavePacket();

/// CMSG_LFG_PROPOSAL_RESULT.
[[nodiscard]] net::wotlk::WorldPacket BuildProposalResultPacket(
    std::uint32_t proposal_id, bool accept);

/// CMSG_LFG_SET_ROLES.
[[nodiscard]] net::wotlk::WorldPacket BuildSetRolesPacket(std::uint8_t roles);

/// CMSG_SET_LFG_COMMENT. The comment ends at its first NUL character.
[[nodiscard]] net::wotlk::WorldPacket BuildSetCommentPacket(std::string_view comment);

/// CMSG_LFG_SET_BOOT_VOTE.
[[nodiscard]] net::wotlk::WorldPacket BuildSetBootVotePacket(bool agree);

/// CMSG_LFG_TELEPORT. `out` true leaves the dungeon, false enters it.
[[nodiscard]] net::wotlk::WorldPacket BuildTeleportPacket(bool out);

}  // namespace openwow::game::lfg
