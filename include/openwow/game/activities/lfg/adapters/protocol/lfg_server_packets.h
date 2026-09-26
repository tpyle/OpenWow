#pragma once

#include "openwow/game/activities/lfg/model/lfg_server_messages.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

/// Decoders for the Dungeon Finder packets the server sends. Each returns
/// nothing when the payload is too short or malformed; trailing bytes are
/// ignored.
namespace openwow::game::lfg {

using Payload = std::span<const std::uint8_t>;

/// SMSG_LFG_JOIN_RESULT. `party_locks` is present when the result is 6
/// (party members are locked out), and lists each member's locks.
struct JoinResultMessage {
  LfgJoinResult result;
  std::optional<std::vector<LfgPartyLockInfo>> party_locks;
};
[[nodiscard]] std::optional<JoinResultMessage> DecodeJoinResult(Payload payload);

/// SMSG_LFG_QUEUE_STATUS.
[[nodiscard]] std::optional<LfgQueueStatus> DecodeQueueStatus(Payload payload);

/// SMSG_LFG_UPDATE_PLAYER. `joined` mirrors `has_extra`.
[[nodiscard]] std::optional<LfgUpdateInfo> DecodeUpdatePlayer(Payload payload);

/// SMSG_LFG_UPDATE_PARTY.
[[nodiscard]] std::optional<LfgUpdateInfo> DecodeUpdateParty(Payload payload);

/// SMSG_LFG_PROPOSAL_UPDATE. At most five players are read.
[[nodiscard]] std::optional<LfgProposal> DecodeProposalUpdate(Payload payload);

/// SMSG_LFG_ROLE_CHECK_UPDATE.
[[nodiscard]] std::optional<LfgRoleCheckUpdate> DecodeRoleCheckUpdate(Payload payload);

/// SMSG_LFG_BOOT_PROPOSAL_UPDATE. Never fails: fields missing from a short
/// payload read as zero and the reason as empty. The reason keeps at most
/// 256 characters (it has no terminator when that long).
[[nodiscard]] LfgBootProposal DecodeBootProposalUpdate(Payload payload);

/// SMSG_LFG_PLAYER_REWARD.
[[nodiscard]] std::optional<LfgPlayerReward> DecodePlayerReward(Payload payload);

/// SMSG_LFG_TELEPORT_DENIED: the denial reason.
[[nodiscard]] std::optional<std::uint32_t> DecodeTeleportDenied(Payload payload);

/// SMSG_LFG_OFFER_CONTINUE: the packed dungeon id.
[[nodiscard]] std::optional<std::uint32_t> DecodeOfferContinue(Payload payload);

/// SMSG_LFG_PLAYER_INFO: per-dungeon reward and lock state. A dungeon listed
/// twice merges into one entry (rewards accumulate, later values win).
[[nodiscard]] std::optional<std::vector<LfgPlayerDungeonState>> DecodePlayerInfo(
    Payload payload);

/// SMSG_LFG_PARTY_INFO: each party member's locks.
[[nodiscard]] std::optional<std::vector<LfgPartyLockInfo>> DecodePartyInfo(Payload payload);

/// SMSG_LFG_ROLE_CHOSEN.
[[nodiscard]] std::optional<LfgRoleChosen> DecodeRoleChosen(Payload payload);

/// SMSG_LFG_UPDATE_SEARCH: the search state byte (0 = not searching).
[[nodiscard]] std::optional<std::uint8_t> DecodeUpdateSearch(Payload payload);

/// SMSG_OPEN_LFG_DUNGEON_FINDER: the dungeon id to show.
[[nodiscard]] std::optional<std::uint32_t> DecodeOpenDungeonFinder(Payload payload);

}  // namespace openwow::game::lfg
