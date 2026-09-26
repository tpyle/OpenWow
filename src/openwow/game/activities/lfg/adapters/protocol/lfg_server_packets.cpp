#include "openwow/game/activities/lfg/adapters/protocol/lfg_server_packets.h"

#include "openwow/game/packet_reader.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

namespace openwow::game::lfg {
namespace {

constexpr std::size_t kBootReasonWireBytesIncludingNul = 0x100;
constexpr std::uint8_t kProposalMaximumPlayerCount = 5;

// Reads a T, or zero once the payload runs out (the cursor then stays past
// the end so every later read also yields zero).
template <typename T>
void ReadPermissiveScalar(const Payload payload, std::size_t& cursor, T& out) {
  out = {};
  if (cursor > payload.size() || payload.size() - cursor < sizeof(T)) {
    cursor = payload.size() + 1;
    return;
  }

  std::memcpy(&out, payload.data() + cursor, sizeof(T));
  cursor += sizeof(T);
}

void ReadPermissiveBootReason(const Payload payload, std::size_t& cursor, std::string& out) {
  out.clear();
  for (std::size_t byte_count = 0;
       byte_count < kBootReasonWireBytesIncludingNul && cursor <= payload.size(); ++byte_count) {
    if (cursor == payload.size()) {
      cursor = payload.size() + 1;
      return;
    }

    const char character = static_cast<char>(payload[cursor++]);
    if (character == '\0') {
      return;
    }
    out += character;
  }
}

LfgPlayerDungeonState& GetOrAppendPlayerDungeonState(std::vector<LfgPlayerDungeonState>& states,
                                                     const std::uint32_t packed_dungeon_id) {
  const auto it = std::find_if(states.begin(), states.end(),
                               [packed_dungeon_id](const LfgPlayerDungeonState& state) {
                                 return state.packed_dungeon_id == packed_dungeon_id;
                               });
  if (it != states.end()) {
    return *it;
  }

  states.push_back(LfgPlayerDungeonState{.packed_dungeon_id = packed_dungeon_id});
  return states.back();
}

LfgPartyLockInfo& GetOrAppendPartyLockInfo(std::vector<LfgPartyLockInfo>& party_locks,
                                           const std::uint64_t guid) {
  const auto it = std::find_if(party_locks.begin(), party_locks.end(),
                               [guid](const LfgPartyLockInfo& entry) { return entry.guid == guid; });
  if (it != party_locks.end()) {
    return *it;
  }

  party_locks.push_back({});
  party_locks.back().guid = guid;
  return party_locks.back();
}

LfgPlayerLock& GetOrAppendJoinResultLock(std::vector<LfgPlayerLock>& player_locks,
                                         const std::uint64_t guid) {
  const auto it = std::find_if(player_locks.begin(), player_locks.end(),
                               [guid](const LfgPlayerLock& entry) {
                                 return entry.player_guid == guid;
                               });
  if (it != player_locks.end()) {
    return *it;
  }

  player_locks.push_back({});
  player_locks.back().player_guid = guid;
  return player_locks.back();
}

void UpsertLockEntry(std::vector<LfgLockEntry>& locks, const std::uint32_t packed_dungeon_id,
                     const std::uint32_t lock_reason) {
  const auto it = std::find_if(locks.begin(), locks.end(),
                               [packed_dungeon_id](const LfgLockEntry& entry) {
                                 return entry.dungeon_entry == packed_dungeon_id;
                               });
  if (it != locks.end()) {
    it->lock_status = lock_reason;
    return;
  }

  locks.push_back({packed_dungeon_id, lock_reason});
}

// A member listed twice merges into one entry; within a member, a dungeon
// listed twice keeps the later reason.
bool ReadPartyLockInfo(PacketReader& reader, std::vector<LfgPartyLockInfo>& out,
                       std::vector<LfgPlayerLock>* join_result_locks = nullptr) {
  std::uint8_t member_count = 0;
  if (!reader.ReadU8(member_count))
    return false;

  std::vector<LfgPartyLockInfo> parsed_party_locks;
  parsed_party_locks.reserve(member_count);
  if (join_result_locks != nullptr) {
    join_result_locks->clear();
    join_result_locks->reserve(member_count);
  }
  for (std::uint8_t i = 0; i < member_count; ++i) {
    std::uint64_t player_guid = 0;
    std::uint32_t lock_count = 0;
    if (!reader.ReadU64(player_guid) || !reader.ReadU32(lock_count))
      return false;

    auto& member_locks = GetOrAppendPartyLockInfo(parsed_party_locks, player_guid);
    member_locks.locks.clear();
    member_locks.locks.reserve(lock_count);

    LfgPlayerLock* player_lock = nullptr;
    if (join_result_locks != nullptr) {
      player_lock = &GetOrAppendJoinResultLock(*join_result_locks, player_guid);
      player_lock->locks.clear();
      player_lock->locks.reserve(lock_count);
    }

    for (std::uint32_t lock_index = 0; lock_index < lock_count; ++lock_index) {
      std::uint32_t packed_dungeon_id = 0;
      std::uint32_t lock_reason = 0;
      if (!reader.ReadU32(packed_dungeon_id) || !reader.ReadU32(lock_reason))
        return false;

      UpsertLockEntry(member_locks.locks, packed_dungeon_id, lock_reason);
      if (player_lock != nullptr) {
        UpsertLockEntry(player_lock->locks, packed_dungeon_id, lock_reason);
      }
    }
  }

  out = std::move(parsed_party_locks);
  return true;
}

std::optional<std::uint32_t> DecodeSingleU32(const Payload payload) {
  PacketReader r(payload);
  std::uint32_t value = 0;
  if (!r.ReadU32(value))
    return std::nullopt;
  return value;
}

}  // namespace

std::optional<JoinResultMessage> DecodeJoinResult(const Payload payload) {
  PacketReader r(payload);
  JoinResultMessage message;
  if (!r.ReadU32(message.result.result) || !r.ReadU32(message.result.state))
    return std::nullopt;

  if (message.result.result == 6) {
    std::vector<LfgPartyLockInfo> parsed_party_locks;
    if (!r.HasBytes(1) ||
        !ReadPartyLockInfo(r, parsed_party_locks, &message.result.player_locks))
      return std::nullopt;
    message.party_locks = std::move(parsed_party_locks);
  }

  return message;
}

std::optional<LfgQueueStatus> DecodeQueueStatus(const Payload payload) {
  PacketReader r(payload);
  LfgQueueStatus qs;
  if (!r.ReadU32(qs.dungeon_id) || !r.ReadI32(qs.wait_time_avg) || !r.ReadI32(qs.wait_time) ||
      !r.ReadI32(qs.wait_time_tank) || !r.ReadI32(qs.wait_time_healer) ||
      !r.ReadI32(qs.wait_time_dps) || !r.ReadU8(qs.tanks_needed) ||
      !r.ReadU8(qs.healers_needed) || !r.ReadU8(qs.dps_needed) || !r.ReadU32(qs.queued_time))
    return std::nullopt;
  return qs;
}

std::optional<LfgUpdateInfo> DecodeUpdatePlayer(const Payload payload) {
  PacketReader r(payload);
  LfgUpdateInfo u;
  std::uint8_t utype = 0;
  std::uint8_t has_extra = 0;
  if (!r.ReadU8(utype) || !r.ReadU8(has_extra))
    return std::nullopt;
  u.update_type = static_cast<LfgUpdateType>(utype);
  u.has_extra = (has_extra != 0);
  u.joined = u.has_extra;

  if (u.has_extra) {
    std::uint8_t queued = 0;
    std::uint8_t raw_flag_4 = 0;
    std::uint8_t raw_flag_5 = 0;
    std::uint8_t dungeon_count = 0;
    if (!r.ReadU8(queued) || !r.ReadU8(raw_flag_4) || !r.ReadU8(raw_flag_5))
      return std::nullopt;
    u.queued = (queued != 0);
    u.raw_flag_4 = (raw_flag_4 != 0);
    u.raw_flag_5 = (raw_flag_5 != 0);
    if (!r.ReadU8(dungeon_count))
      return std::nullopt;
    u.dungeons.reserve(dungeon_count);
    for (std::uint8_t i = 0; i < dungeon_count; ++i) {
      std::uint32_t packed_dungeon_id = 0;
      if (!r.ReadU32(packed_dungeon_id))
        return std::nullopt;
      u.AddDungeonSelection(packed_dungeon_id);
    }
    if (!r.ReadCString(u.comment))
      return std::nullopt;
  }

  return u;
}

std::optional<LfgUpdateInfo> DecodeUpdateParty(const Payload payload) {
  PacketReader r(payload);
  LfgUpdateInfo u;
  std::uint8_t utype = 0;
  std::uint8_t has_extra = 0;
  if (!r.ReadU8(utype) || !r.ReadU8(has_extra))
    return std::nullopt;
  u.update_type = static_cast<LfgUpdateType>(utype);
  u.has_extra = (has_extra != 0);

  if (u.has_extra) {
    std::uint8_t joined = 0;
    std::uint8_t queued = 0;
    std::uint8_t raw_flag_4 = 0;
    std::uint8_t raw_flag_5 = 0;
    if (!r.ReadU8(joined) || !r.ReadU8(queued) || !r.ReadU8(raw_flag_4) || !r.ReadU8(raw_flag_5))
      return std::nullopt;
    u.joined = (joined != 0);
    u.queued = (queued != 0);
    u.raw_flag_4 = (raw_flag_4 != 0);
    u.raw_flag_5 = (raw_flag_5 != 0);
    for (auto& tail_byte : u.raw_tail_bytes) {
      if (!r.ReadU8(tail_byte))
        return std::nullopt;
    }
    std::uint8_t dungeon_count = 0;
    if (!r.ReadU8(dungeon_count))
      return std::nullopt;
    u.dungeons.reserve(dungeon_count);
    for (std::uint8_t i = 0; i < dungeon_count; ++i) {
      std::uint32_t packed_dungeon_id = 0;
      if (!r.ReadU32(packed_dungeon_id))
        return std::nullopt;
      u.AddDungeonSelection(packed_dungeon_id);
    }
    if (!r.ReadCString(u.comment))
      return std::nullopt;
  }

  return u;
}

std::optional<LfgProposal> DecodeProposalUpdate(const Payload payload) {
  PacketReader r(payload);
  LfgProposal p;
  if (!r.ReadU32(p.dungeon_entry) || !r.ReadU8(p.state) || !r.ReadU32(p.proposal_id) ||
      !r.ReadU32(p.encounter_mask))
    return std::nullopt;

  std::uint8_t silent = 0;
  std::uint8_t player_count = 0;
  if (!r.ReadU8(silent) || !r.ReadU8(player_count))
    return std::nullopt;
  p.silent = (silent != 0);

  const auto parsed_player_count = std::min(player_count, kProposalMaximumPlayerCount);
  p.players.resize(parsed_player_count);
  for (std::uint8_t i = 0; i < parsed_player_count; ++i) {
    auto& pl = p.players[i];
    if (!r.ReadU32(pl.role))
      return std::nullopt;
    std::uint8_t cur = 0;
    std::uint8_t dun = 0;
    std::uint8_t sg = 0;
    std::uint8_t ans = 0;
    std::uint8_t acc = 0;
    if (!r.ReadU8(cur) || !r.ReadU8(dun) || !r.ReadU8(sg) || !r.ReadU8(ans) || !r.ReadU8(acc))
      return std::nullopt;
    pl.is_current_player = (cur != 0);
    pl.in_dungeon = (dun != 0);
    pl.same_group = (sg != 0);
    pl.has_answered = (ans != 0);
    pl.has_accepted = (acc != 0);
  }

  return p;
}

std::optional<LfgRoleCheckUpdate> DecodeRoleCheckUpdate(const Payload payload) {
  PacketReader r(payload);
  LfgRoleCheckUpdate rc;
  std::uint8_t beginning = 0;
  std::uint8_t dungeon_count = 0;
  if (!r.ReadU32(rc.state) || !r.ReadU8(beginning) || !r.ReadU8(dungeon_count))
    return std::nullopt;
  rc.is_beginning = (beginning != 0);

  rc.dungeons.resize(dungeon_count);
  for (std::uint8_t i = 0; i < dungeon_count; ++i)
    if (!r.ReadU32(rc.dungeons[i]))
      return std::nullopt;

  std::uint8_t player_count = 0;
  if (!r.ReadU8(player_count))
    return std::nullopt;
  rc.players.resize(player_count);
  for (std::uint8_t i = 0; i < player_count; ++i) {
    auto& p = rc.players[i];
    std::uint8_t ready = 0;
    if (!r.ReadU64(p.guid) || !r.ReadU8(ready) || !r.ReadU32(p.roles) || !r.ReadU8(p.level))
      return std::nullopt;
    p.ready = (ready != 0);
  }

  return rc;
}

LfgBootProposal DecodeBootProposalUpdate(const Payload payload) {
  LfgBootProposal bp;
  std::size_t cursor = 0;
  std::uint8_t in_prog = 0;
  std::uint8_t did_vote = 0;
  std::uint8_t agree = 0;
  ReadPermissiveScalar(payload, cursor, in_prog);
  ReadPermissiveScalar(payload, cursor, did_vote);
  ReadPermissiveScalar(payload, cursor, agree);
  bp.in_progress = (in_prog != 0);
  bp.did_vote = (did_vote != 0);
  bp.agree = (agree != 0);
  ReadPermissiveScalar(payload, cursor, bp.victim_guid);
  ReadPermissiveScalar(payload, cursor, bp.total_votes);
  ReadPermissiveScalar(payload, cursor, bp.agree_count);
  ReadPermissiveScalar(payload, cursor, bp.time_left);
  ReadPermissiveScalar(payload, cursor, bp.needed_votes);
  ReadPermissiveBootReason(payload, cursor, bp.reason);
  return bp;
}

std::optional<LfgPlayerReward> DecodePlayerReward(const Payload payload) {
  PacketReader r(payload);
  LfgPlayerReward rew;
  std::uint8_t is_first_reward = 0;
  if (!r.ReadU32(rew.random_dungeon_entry) || !r.ReadU32(rew.completed_dungeon_entry) ||
      !r.ReadU8(is_first_reward))
    return std::nullopt;
  rew.is_first_reward = (is_first_reward != 0);
  if (!r.ReadU32(rew.strangers_count) || !r.ReadU32(rew.base_money_reward) ||
      !r.ReadU32(rew.base_xp_reward) || !r.ReadU32(rew.variable_money_reward) ||
      !r.ReadU32(rew.variable_xp_reward))
    return std::nullopt;

  std::uint8_t item_count = 0;
  if (!r.ReadU8(item_count))
    return std::nullopt;
  rew.items.resize(item_count);
  for (std::uint8_t i = 0; i < item_count; ++i) {
    auto& it = rew.items[i];
    if (!r.ReadU32(it.item_id) || !r.ReadU32(it.display_info_id) || !r.ReadU32(it.item_count))
      return std::nullopt;
  }

  return rew;
}

std::optional<std::uint32_t> DecodeTeleportDenied(const Payload payload) {
  return DecodeSingleU32(payload);
}

std::optional<std::uint32_t> DecodeOfferContinue(const Payload payload) {
  return DecodeSingleU32(payload);
}

std::optional<std::vector<LfgPlayerDungeonState>> DecodePlayerInfo(const Payload payload) {
  PacketReader reader(payload);
  std::vector<LfgPlayerDungeonState> parsed_states;

  std::uint8_t dungeon_count = 0;
  if (!reader.ReadU8(dungeon_count)) {
    return std::nullopt;
  }
  parsed_states.reserve(dungeon_count);

  for (std::uint8_t i = 0; i < dungeon_count; ++i) {
    std::uint32_t packed_dungeon_id = 0;
    std::uint8_t reward_done = 0;
    std::uint32_t reward_money = 0;
    std::uint32_t reward_xp = 0;
    std::uint32_t reward_money_var = 0;
    std::uint32_t reward_xp_var = 0;
    std::uint8_t reward_count = 0;
    if (!reader.ReadU32(packed_dungeon_id) || !reader.ReadU8(reward_done) ||
        !reader.ReadU32(reward_money) || !reader.ReadU32(reward_xp) ||
        !reader.ReadU32(reward_money_var) || !reader.ReadU32(reward_xp_var) ||
        !reader.ReadU8(reward_count)) {
      return std::nullopt;
    }

    auto& state = GetOrAppendPlayerDungeonState(parsed_states, packed_dungeon_id);
    state.reward_done = reward_done != 0;
    state.reward_money = reward_money;
    state.reward_xp = reward_xp;
    state.reward_money_var = reward_money_var;
    state.reward_xp_var = reward_xp_var;

    state.rewards.reserve(state.rewards.size() + reward_count);
    for (std::uint8_t reward_index = 0; reward_index < reward_count; ++reward_index) {
      LfgRewardItem reward{};
      if (!reader.ReadU32(reward.item_id) || !reader.ReadU32(reward.display_info_id) ||
          !reader.ReadU32(reward.item_count)) {
        return std::nullopt;
      }
      state.rewards.push_back(reward);
    }
  }

  std::uint32_t locked_count = 0;
  if (!reader.ReadU32(locked_count)) {
    return std::nullopt;
  }

  for (std::uint32_t i = 0; i < locked_count; ++i) {
    std::uint32_t packed_dungeon_id = 0;
    std::uint32_t lock_reason = 0;
    if (!reader.ReadU32(packed_dungeon_id) || !reader.ReadU32(lock_reason)) {
      return std::nullopt;
    }

    auto& state = GetOrAppendPlayerDungeonState(parsed_states, packed_dungeon_id);
    state.locked = true;
    state.lock_reason = lock_reason;
  }

  return parsed_states;
}

std::optional<std::vector<LfgPartyLockInfo>> DecodePartyInfo(const Payload payload) {
  PacketReader reader(payload);
  std::vector<LfgPartyLockInfo> parsed_party_locks;
  if (!ReadPartyLockInfo(reader, parsed_party_locks))
    return std::nullopt;
  return parsed_party_locks;
}

std::optional<LfgRoleChosen> DecodeRoleChosen(const Payload payload) {
  PacketReader r(payload);
  LfgRoleChosen rc{};
  if (!r.ReadU64(rc.guid) || !r.ReadU8(rc.ready) || !r.ReadU32(rc.roles))
    return std::nullopt;
  return rc;
}

std::optional<std::uint8_t> DecodeUpdateSearch(const Payload payload) {
  PacketReader r(payload);
  std::uint8_t state = 0;
  if (!r.ReadU8(state))
    return std::nullopt;
  return state;
}

std::optional<std::uint32_t> DecodeOpenDungeonFinder(const Payload payload) {
  return DecodeSingleU32(payload);
}

}  // namespace openwow::game::lfg
