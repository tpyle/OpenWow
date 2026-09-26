
#include "openwow/game/lfg_manager.h"

#include "openwow/game/activities/lfg/adapters/protocol/lfg_server_packets.h"
#include "openwow/game/activities/lfg/rules/lfg_dungeon_rules.h"

#include "openwow/core/storm_string.h"
#include "openwow/data/formats/dbc/dbc_loader.h"
#include "openwow/data/formats/dbc/dbc_table_registry.h"
#include "openwow/game/packet_reader.h"
#include "openwow/game/unit_query_bridge.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <unordered_map>

namespace openwow::game {


namespace {

constexpr std::uint32_t kPackedDungeonIdMask = 0x00FFFFFFu;
constexpr std::size_t kSearchSortStringLimit = 0x7FFFFFFFu;
constexpr std::size_t kLfgSearchCommentMaxBytesIncludingNul = 0x100;

bool ReadBoundedCString(PacketReader &reader, std::string &out) {
  return reader.ReadCString(out, kLfgSearchCommentMaxBytesIncludingNul);
}

bool MatchesDungeonId(const std::uint32_t packed_dungeon_id, const std::uint32_t dungeon_id) {
  return (packed_dungeon_id & kPackedDungeonIdMask) == dungeon_id;
}

bool ReadSearchGroupDelta(PacketReader &reader, LfgSearchGroupResult &group) {
  std::uint32_t mask = 0;
  if (!reader.ReadU32(mask))
    return false;

  if ((mask & 0x2u) != 0 && !ReadBoundedCString(reader, group.comment)) {
    return false;
  }

  if ((mask & 0x10u) != 0) {
    for (auto &value : group.raw_u8_292_294) {
      if (!reader.ReadU8(value))
        return false;
    }
  }

  if ((mask & 0x80u) != 0) {
    if (!reader.ReadU64(group.encounter_guid))
      return false;
    if (!reader.ReadU32(group.encounter_mask))
      return false;
  }

  return true;
}

bool ReadSearchPlayerDelta(PacketReader &reader, LfgSearchPlayerResult &player,
                           std::uint32_t &mask) {
  if (!reader.ReadU32(mask))
    return false;

  if ((mask & 0x1u) != 0) {
    if (!reader.ReadU8(player.level) || !reader.ReadU8(player.raw_u8_45) ||
        !reader.ReadU8(player.raw_u8_46)) {
      return false;
    }

    for (auto &value : player.raw_u8_47_49) {
      if (!reader.ReadU8(value))
        return false;
    }
    for (auto &value : player.raw_u32_52_72) {
      if (!reader.ReadU32(value))
        return false;
    }
    for (auto &value : player.raw_f32_76_80) {
      if (!reader.ReadFloat(value))
        return false;
    }
    for (auto &value : player.raw_u32_84_100) {
      if (!reader.ReadU32(value))
        return false;
    }
    if (!reader.ReadFloat(player.raw_f32_104))
      return false;
    for (auto &value : player.raw_u32_108_128) {
      if (!reader.ReadU32(value))
        return false;
    }
  }

  if ((mask & 0x2u) != 0 && !ReadBoundedCString(reader, player.comment)) {
    return false;
  }

  if ((mask & 0x4u) != 0) {
    std::uint8_t joined = 0;
    if (!reader.ReadU8(joined))
      return false;
    player.joined_group = joined != 0;
  }

  if ((mask & 0x8u) != 0) {
    if (!reader.ReadU64(player.resolved_group_guid))
      return false;
  }

  if ((mask & 0x10u) != 0) {
    if (!reader.ReadU8(player.search_flags))
      return false;
  }

  if ((mask & 0x20u) != 0) {
    if (!reader.ReadU32(player.area_id))
      return false;
  }

  if ((mask & 0x40u) != 0) {
    if (!reader.ReadU8(player.role_byte))
      return false;
  }

  if ((mask & 0x80u) != 0) {
    if (!reader.ReadU64(player.secondary_guid))
      return false;
    if (!reader.ReadU32(player.secondary_mask))
      return false;
  }

  return true;
}

bool HasWorkingSearchGroup(const std::unordered_map<std::uint64_t, LfgSearchGroupResult> &groups,
                           const std::uint64_t guid) {
  return guid != 0 && groups.contains(guid);
}

std::uint32_t PackDungeonId(const openwow::data::dbc::LfgDungeonsEntry &entry) {
  return lfg::PackDungeonId(entry.id, entry.type_id);
}

std::uint32_t EffectiveTargetAverage(const openwow::data::dbc::LfgDungeonsEntry &entry,
                                     const openwow::data::dbc::LfgDungeonExpansionEntry *override) {
  const auto min_target = override != nullptr ? override->target_level_min : entry.rec_min_level;
  const auto max_target = override != nullptr ? override->target_level_max : entry.rec_max_level;
  return (min_target + max_target) / 2;
}

std::uint32_t EffectiveMinLevel(const openwow::data::dbc::LfgDungeonsEntry &entry,
                                const openwow::data::dbc::LfgDungeonExpansionEntry *override) {
  return override != nullptr ? override->hard_level_min : entry.min_level;
}

void EraseGuid(std::vector<std::uint64_t> &guids, const std::uint64_t guid) {
  guids.erase(std::remove(guids.begin(), guids.end(), guid), guids.end());
}

std::optional<UnitQuerySnapshot> ResolveSearchSnapshot(WorldSession *session,
                                                       const std::uint64_t guid);

const char *StormStringOrEmpty(const std::string_view value) {
  return value.empty() ? "" : value.data();
}

struct SearchSortResolver {
  SearchSortResolver(WorldSession *session, const openwow::data::dbc::DbcLoader *dbc)
      : session_(session), dbc_(dbc) {}

  int Compare(const LfgSearchPlayerResult &left, const LfgSearchPlayerResult &right,
              const LfgSearchSortKey key, const bool include_role_flags) {
    switch (key) {
    case LfgSearchSortKey::kZone: {
      const auto left_name = LookupAreaName(left.area_id);
      const auto right_name = LookupAreaName(right.area_id);
      if (!left_name.has_value() || !right_name.has_value()) {
        return 0;
      }
      return openwow::core::SStrCmpNoCaseCollate(
          StormStringOrEmpty(left_name.value()), StormStringOrEmpty(right_name.value()),
          kSearchSortStringLimit);
    }
    case LfgSearchSortKey::kLevel:
      if (left.level == right.level) {
        return 0;
      }
      return left.level < right.level ? -1 : 1;
    case LfgSearchSortKey::kClass: {
      const auto *left_snapshot = LookupSnapshot(left.guid);
      const auto *right_snapshot = LookupSnapshot(right.guid);
      if (left_snapshot == nullptr || right_snapshot == nullptr) {
        return 0;
      }

      const auto left_name = LookupClassName(left_snapshot->classId);
      const auto right_name = LookupClassName(right_snapshot->classId);
      if (!left_name.has_value() || !right_name.has_value()) {
        return 0;
      }
      return openwow::core::SStrCmpNoCaseCollate(
          StormStringOrEmpty(left_name.value()), StormStringOrEmpty(right_name.value()),
          kSearchSortStringLimit);
    }
    case LfgSearchSortKey::kName: {
      const auto *left_snapshot = LookupSnapshot(left.guid);
      const auto *right_snapshot = LookupSnapshot(right.guid);
      if (left_snapshot == nullptr || right_snapshot == nullptr) {
        return 0;
      }
      return openwow::core::SStrCmpI(left_snapshot->name.c_str(), right_snapshot->name.c_str(),
                                     kSearchSortStringLimit);
    }
    case LfgSearchSortKey::kTank:
      if (!include_role_flags) {
        return 0;
      }
      return static_cast<int>(right.search_flags & 0x02u) -
             static_cast<int>(left.search_flags & 0x02u);
    case LfgSearchSortKey::kHealer:
      if (!include_role_flags) {
        return 0;
      }
      return static_cast<int>(right.search_flags & 0x04u) -
             static_cast<int>(left.search_flags & 0x04u);
    case LfgSearchSortKey::kDamage:
      if (!include_role_flags) {
        return 0;
      }
      return static_cast<int>(right.search_flags & 0x08u) -
             static_cast<int>(left.search_flags & 0x08u);
    }
    return 0;
  }

private:
  const UnitQuerySnapshot *LookupSnapshot(const std::uint64_t guid) {
    const auto [it, inserted] = snapshot_cache_.try_emplace(guid);
    if (inserted) {
      it->second = ResolveSearchSnapshot(session_, guid);
    }
    return it->second.has_value() ? &it->second.value() : nullptr;
  }

  std::optional<std::string_view> LookupAreaName(const std::uint32_t area_id) const {
    if (dbc_ == nullptr) {
      return std::nullopt;
    }

    const auto *area = dbc_->area_table().LookupEntry(area_id);
    if (area == nullptr) {
      return std::nullopt;
    }
    return area->name;
  }

  std::optional<std::string_view> LookupClassName(const std::uint8_t class_id) const {
    if (dbc_ == nullptr) {
      return std::nullopt;
    }

    const auto *chr_class = dbc_->chr_classes().LookupEntry(class_id);
    if (chr_class == nullptr) {
      return std::nullopt;
    }
    return chr_class->name;
  }

  WorldSession *session_;
  const openwow::data::dbc::DbcLoader *dbc_;
  std::unordered_map<std::uint64_t, std::optional<UnitQuerySnapshot>> snapshot_cache_{};
};

std::optional<UnitQuerySnapshot> ResolveSearchSnapshot(WorldSession *session, const std::uint64_t guid) {
  if (session == nullptr || guid == 0)
    return std::nullopt;
  return UnitQueryBridge::Get().GetPlayerInfoByGUID(session, guid);
}

}

bool LfgManager::HandleLfgJoinResult(const std::uint8_t *data, std::size_t len) {
  auto message = lfg::DecodeJoinResult({data, len});
  if (!message)
    return false;
  if (message->party_locks) {
    party_lock_info_ = std::move(*message->party_locks);
    has_party_lock_info_ = true;
  }
  join_result_ = std::move(message->result);
  return true;
}

bool LfgManager::HandleLfgQueueStatus(const std::uint8_t *data, std::size_t len) {
  auto qs = lfg::DecodeQueueStatus({data, len});
  if (!qs)
    return false;
  queue_status_ = *qs;
  return true;
}

void LfgManager::ClearQueueStatus() {
  queue_status_.reset();
}

bool LfgManager::HandleLfgUpdatePlayer(const std::uint8_t *data, std::size_t len) {
  auto u = lfg::DecodeUpdatePlayer({data, len});
  if (!u)
    return false;
  player_update_ = std::move(*u);
  return true;
}

bool LfgManager::HandleLfgUpdateParty(const std::uint8_t *data, std::size_t len) {
  auto u = lfg::DecodeUpdateParty({data, len});
  if (!u)
    return false;
  party_update_ = std::move(*u);
  return true;
}

bool LfgManager::HandleLfgProposalUpdate(const std::uint8_t *data, std::size_t len) {
  auto p = lfg::DecodeProposalUpdate({data, len});
  if (!p)
    return false;

  // A new proposal id shows the proposal once, unless the server marks it
  // silent; repeated updates for the same proposal don't.
  proposal_show_pending_ = false;
  if (last_proposal_event_id_ != p->proposal_id) {
    last_proposal_event_id_ = p->proposal_id;
    proposal_show_pending_ = !p->silent;
  }
  proposal_ = std::move(*p);
  return true;
}

bool LfgManager::HandleLfgRoleCheckUpdate(const std::uint8_t *data, std::size_t len) {
  auto rc = lfg::DecodeRoleCheckUpdate({data, len});
  if (!rc)
    return false;
  role_check_ = std::move(*rc);
  return true;
}

bool LfgManager::HandleLfgBootProposalUpdate(const std::uint8_t *data, std::size_t len) {
  boot_proposal_ = lfg::DecodeBootProposalUpdate({data, len});
  return true;
}

bool LfgManager::HandleLfgPlayerReward(const std::uint8_t *data, std::size_t len) {
  // Any reward packet, even a malformed one, drops the previous reward.
  ClearPlayerReward();
  auto reward = lfg::DecodePlayerReward({data, len});
  if (!reward)
    return false;
  player_reward_ = std::move(*reward);
  return true;
}

bool LfgManager::HandleLfgTeleportDenied(const std::uint8_t *data, std::size_t len) {
  const auto reason = lfg::DecodeTeleportDenied({data, len});
  if (!reason)
    return false;
  teleport_error_ = *reason;
  return true;
}

bool LfgManager::HandleLfgOfferContinue(const std::uint8_t *data, std::size_t len) {
  const auto dungeon = lfg::DecodeOfferContinue({data, len});
  if (!dungeon)
    return false;
  offer_continue_dungeon_ = *dungeon;
  return true;
}

void LfgManager::ApplyProposalResponse(bool accept) {
  if (!proposal_.has_value()) {
    return;
  }

  if (!accept) {
    proposal_.reset();
    return;
  }

  for (auto& player : proposal_->players) {
    if (!player.is_current_player) {
      continue;
    }

    player.has_answered = true;
    player.has_accepted = true;
  }
}

void LfgManager::ClearProposal() {
  proposal_.reset();
  proposal_show_pending_ = false;
}

void LfgManager::ResetProposalEventGateForPlayerEnterWorld() {
  last_proposal_event_id_ = 0;
  proposal_show_pending_ = false;
}

bool LfgManager::ConsumeProposalShowPending() {
  const bool pending = proposal_show_pending_;
  proposal_show_pending_ = false;
  return pending;
}

void LfgManager::ClearServerInfoSnapshots() {
  player_update_.reset();
  party_update_.reset();
}

void LfgManager::ClearPlayerReward() {
  player_reward_.reset();
}

void LfgManager::Clear() {
  join_result_.reset();
  ClearQueueStatus();
  ClearServerInfoSnapshots();
  proposal_.reset();
  last_proposal_event_id_ = 0;
  proposal_show_pending_ = false;
  role_check_.reset();
  boot_proposal_.reset();
  ClearPlayerReward();
  teleport_error_ = 0;
  offer_continue_dungeon_ = 0;
  lfg_player_info_blob_.clear();
  lfg_party_info_blob_.clear();
  last_role_chosen_.reset();
  has_player_dungeon_info_ = false;
  has_party_lock_info_ = false;
  player_dungeon_states_.clear();
  party_lock_info_.clear();
  lfg_update_search_ = 0;
  lfg_disabled_ = false;
  open_lfg_dungeon_id_ = 0;
  update_lfg_list_blob_.clear();
  joined_search_id_ = 0;
  ResetSearchResults();
}

bool LfgManager::HandleLfgPlayerInfo(const std::uint8_t *data, std::size_t len) {
  // The raw payload is kept even when it doesn't decode.
  lfg_player_info_blob_.assign(data, data + len);
  auto states = lfg::DecodePlayerInfo({data, len});
  if (!states)
    return false;
  player_dungeon_states_ = std::move(*states);
  has_player_dungeon_info_ = true;
  return true;
}

bool LfgManager::HandleLfgPartyInfo(const std::uint8_t *data, std::size_t len) {
  // The raw payload is kept even when it doesn't decode.
  lfg_party_info_blob_.assign(data, data + len);
  auto locks = lfg::DecodePartyInfo({data, len});
  if (!locks)
    return false;
  party_lock_info_ = std::move(*locks);
  has_party_lock_info_ = true;
  return true;
}

const LfgPlayerDungeonState *LfgManager::FindPlayerDungeonState(
    const std::uint32_t packed_dungeon_id) const {
  const auto it = std::find_if(player_dungeon_states_.begin(), player_dungeon_states_.end(),
                               [packed_dungeon_id](const LfgPlayerDungeonState &state) {
                                 return state.packed_dungeon_id == packed_dungeon_id;
                               });
  if (it == player_dungeon_states_.end()) {
    return nullptr;
  }
  return &*it;
}

const LfgRewardItem *LfgManager::FindCompletionRewardItemByIndex(
    const std::size_t reward_index) const {
  if (!player_reward_.has_value() || reward_index >= player_reward_->items.size()) {
    return nullptr;
  }

  return &player_reward_->items[reward_index];
}

const LfgRewardItem *LfgManager::FindPlayerDungeonRewardItemByIndex(
    const std::uint32_t packed_dungeon_id, const std::size_t reward_index) const {
  if (!has_player_dungeon_info_) {
    return nullptr;
  }

  const auto *state = FindPlayerDungeonState(packed_dungeon_id);
  if (state == nullptr || reward_index >= state->rewards.size()) {
    return nullptr;
  }

  return &state->rewards[reward_index];
}

std::size_t LfgManager::GetLfdLockPlayerCount() const {
  if (!has_player_dungeon_info_)
    return 0;
  return has_party_lock_info_ ? party_lock_info_.size() + 1 : 1;
}

std::optional<std::uint32_t> LfgManager::FindPlayerLockReason(
    const std::uint32_t dungeon_id) const {
  for (const auto &state : player_dungeon_states_) {
    if (state.locked && MatchesDungeonId(state.packed_dungeon_id, dungeon_id))
      return state.lock_reason;
  }
  return std::nullopt;
}

const LfgPartyLockInfo *LfgManager::GetPartyLockInfo(const std::size_t member_index) const {
  if (member_index >= party_lock_info_.size())
    return nullptr;
  return &party_lock_info_[member_index];
}

const LfgPartyLockInfo *LfgManager::FindPartyLockByGuid(const std::uint64_t guid) const {
  for (const auto &info : party_lock_info_) {
    if (info.guid == guid)
      return &info;
  }
  return nullptr;
}

std::optional<std::uint32_t> LfgManager::FindPartyLockReason(
    const std::size_t member_index, const std::uint32_t dungeon_id) const {
  const auto *member_locks = GetPartyLockInfo(member_index);
  if (member_locks == nullptr)
    return std::nullopt;

  for (const auto &lock : member_locks->locks) {
    if (MatchesDungeonId(lock.dungeon_entry, dungeon_id))
      return lock.lock_status;
  }
  return std::nullopt;
}

std::vector<LfgLockEntry> LfgManager::GetChoiceLockEntries() const {
  std::vector<LfgLockEntry> locks;

  if (has_party_lock_info_) {
    for (const auto &member : party_lock_info_) {
      for (const auto &lock : member.locks) {
        if ((lock.dungeon_entry & 0xFF000000u) == 0x02000000u) {
          continue;
        }
        locks.push_back(lock);
      }
    }
  }

  if (has_player_dungeon_info_) {
    for (const auto &state : player_dungeon_states_) {
      if (!state.locked) {
        continue;
      }
      locks.push_back({state.packed_dungeon_id, state.lock_reason});
    }
  }

  return locks;
}

bool LfgManager::HasUnlockedPlayerDungeon(const std::uint32_t packed_dungeon_id) const {
  const auto it = std::find_if(player_dungeon_states_.begin(), player_dungeon_states_.end(),
                               [packed_dungeon_id](const LfgPlayerDungeonState &state) {
                                 return state.packed_dungeon_id == packed_dungeon_id;
                               });
  return it != player_dungeon_states_.end() && !it->locked;
}

bool LfgManager::IsDungeonJoinable(const std::uint32_t packed_dungeon_id) const {
  if (has_player_dungeon_info_) {
    const auto it = std::find_if(player_dungeon_states_.begin(), player_dungeon_states_.end(),
                                 [packed_dungeon_id](const LfgPlayerDungeonState &state) {
                                   return state.packed_dungeon_id == packed_dungeon_id;
                                 });
    if (it != player_dungeon_states_.end() && it->locked) {
      return false;
    }
  }

  if ((packed_dungeon_id & 0xFF000000u) != 0x02000000u && has_party_lock_info_) {
    for (const auto &member_locks : party_lock_info_) {
      const auto lock_it = std::find_if(member_locks.locks.begin(), member_locks.locks.end(),
                                        [packed_dungeon_id](const LfgLockEntry &lock) {
                                          return lock.dungeon_entry == packed_dungeon_id;
                                        });
      if (lock_it != member_locks.locks.end()) {
        return false;
      }
    }
  }

  return true;
}

std::vector<std::uint32_t> LfgManager::GetAvailableRandomDungeonIds(
    const openwow::data::dbc::DbcLoader &dbc) const {
  std::vector<std::uint32_t> dungeon_ids;
  for (const auto &entry : dbc.lfg_dungeons()) {
    const auto packed_dungeon_id = PackDungeonId(entry);
    if (entry.type_id == 6u ||
        ((entry.flags & 0x4u) != 0 && HasUnlockedPlayerDungeon(packed_dungeon_id))) {
      dungeon_ids.push_back(entry.id);
    }
  }
  return dungeon_ids;
}

std::optional<std::uint32_t> LfgManager::GetBestRandomDungeonId(
    const openwow::data::dbc::DbcLoader &dbc, const std::uint8_t expansion_level) const {
  const openwow::data::dbc::LfgDungeonsEntry *best_entry = nullptr;
  const openwow::data::dbc::LfgDungeonExpansionEntry *best_override = nullptr;

  for (const auto &entry : dbc.lfg_dungeons()) {
    if (entry.type_id != 6u) {
      continue;
    }

    if (!IsDungeonJoinable(PackDungeonId(entry))) {
      continue;
    }

    const auto *entry_override = openwow::data::DBClient_FindLfgDungeonExpansion(
        &dbc, entry.id, expansion_level);
    if (best_entry == nullptr) {
      best_entry = &entry;
      best_override = entry_override;
      continue;
    }

    if (entry.expansion_level != best_entry->expansion_level) {
      if (entry.expansion_level > best_entry->expansion_level) {
        best_entry = &entry;
        best_override = entry_override;
      }
      continue;
    }

    const auto entry_average = EffectiveTargetAverage(entry, entry_override);
    const auto best_average = EffectiveTargetAverage(*best_entry, best_override);
    if (entry_average != best_average) {
      if (entry_average > best_average) {
        best_entry = &entry;
        best_override = entry_override;
      }
      continue;
    }

    if (EffectiveMinLevel(entry, entry_override) > EffectiveMinLevel(*best_entry, best_override)) {
      best_entry = &entry;
      best_override = entry_override;
    }
  }

  if (best_entry == nullptr) {
    return std::nullopt;
  }

  return best_entry->id;
}

bool LfgManager::HandleLfgRoleChosen(const std::uint8_t *data, std::size_t len) {
  const auto rc = lfg::DecodeRoleChosen({data, len});
  if (!rc)
    return false;
  last_role_chosen_ = *rc;
  return true;
}

bool LfgManager::HandleLfgUpdateSearch(const std::uint8_t *data, std::size_t len) {
  const auto state = lfg::DecodeUpdateSearch({data, len});
  if (!state)
    return false;
  lfg_update_search_ = *state;

  if (lfg_update_search_ == 0 && joined_search_id_ != 0) {
    update_lfg_list_blob_.clear();
    ResetSearchResultData();
    joined_search_id_ = 0;
  }

  return true;
}

bool LfgManager::HandleLfgDisabled(const std::uint8_t * , std::size_t ) {
  Clear();
  lfg_disabled_ = true;
  return true;
}

bool LfgManager::HandleOpenLfgDungeonFinder(const std::uint8_t *data, std::size_t len) {
  const auto dungeon = lfg::DecodeOpenDungeonFinder({data, len});
  if (!dungeon)
    return false;
  open_lfg_dungeon_id_ = *dungeon;
  return true;
}

bool LfgManager::HandleUpdateLfgList(const std::uint8_t *data, std::size_t len) {
  PacketReader reader(data, len);
  pending_search_player_name_query_requests_.clear();

  std::uint32_t type_id = 0;
  std::uint32_t dungeon_id = 0;
  if (!reader.ReadU32(type_id) || !reader.ReadU32(dungeon_id))
    return false;

  const auto packed_search_id = (type_id << 24) | (dungeon_id & 0x00FFFFFFu);
  if (packed_search_id != joined_search_id_) {
    return true;
  }

  update_lfg_list_blob_.assign(data, data + len);
  const bool should_publish = working_search_id_ != packed_search_id;

  std::uint8_t has_delete_list = 0;
  if (!reader.ReadU8(has_delete_list))
    return false;

  if (has_delete_list == 0) {
    ResetWorkingSearchResults();
  } else {
    std::uint32_t delete_count = 0;
    if (!reader.ReadU32(delete_count))
      return false;
    for (std::uint32_t i = 0; i < delete_count; ++i) {
      std::uint64_t guid = 0;
      if (!reader.ReadU64(guid))
        return false;
      RemoveWorkingSearchResult(guid);
    }
  }

  working_search_id_ = packed_search_id;

  std::uint32_t group_count = 0;
  if (!reader.ReadU32(group_count) || !reader.ReadU32(working_reported_group_total_)) {
    return false;
  }
  for (std::uint32_t i = 0; i < group_count; ++i) {
    std::uint64_t guid = 0;
    if (!reader.ReadU64(guid))
      return false;

    const auto [it, inserted] = working_search_groups_.try_emplace(guid);
    auto &group = it->second;
    group.guid = guid;
    if (inserted) {
      working_group_order_.push_back(guid);
    }
    if (!ReadSearchGroupDelta(reader, group))
      return false;
  }

  std::uint32_t player_count = 0;
  if (!reader.ReadU32(player_count) || !reader.ReadU32(working_reported_player_total_)) {
    return false;
  }
  for (std::uint32_t i = 0; i < player_count; ++i) {
    std::uint64_t guid = 0;
    if (!reader.ReadU64(guid))
      return false;

    const auto [it, inserted] = working_search_players_.try_emplace(guid);
    auto &player = it->second;
    const auto old_resolved_group_guid = player.resolved_group_guid;
    const bool old_joined_group = player.joined_group;
    std::uint32_t mask = 0;
    player.guid = guid;
    if (!ReadSearchPlayerDelta(reader, player, mask))
      return false;
    if (inserted) {
      pending_search_player_name_query_requests_.push_back(guid);
    }

    bool placement_updated = false;
    if ((mask & 0x8u) != 0) {
      const auto requested_group_guid = player.resolved_group_guid;
      player.resolved_group_guid = old_resolved_group_guid;

      if (old_resolved_group_guid != requested_group_guid) {
        if (old_resolved_group_guid != 0) {
          RemoveWorkingGroupMember(old_resolved_group_guid, guid);
        } else {
          RemoveWorkingStandalonePlayer(guid);
        }

        if (HasWorkingSearchGroup(working_search_groups_, requested_group_guid)) {
          player.resolved_group_guid = requested_group_guid;
          InsertWorkingGroupMember(player.resolved_group_guid, guid, player.joined_group);
        } else {
          player.resolved_group_guid = 0;
          AppendWorkingStandalonePlayer(guid);
        }
        placement_updated = true;
      }
    }

    if (!placement_updated && player.resolved_group_guid != 0 &&
        old_joined_group != player.joined_group) {
      RemoveWorkingGroupMember(player.resolved_group_guid, guid);
      InsertWorkingGroupMember(player.resolved_group_guid, guid, player.joined_group);
      placement_updated = true;
    }

    if (!placement_updated && inserted && player.resolved_group_guid == 0) {
      AppendWorkingStandalonePlayer(guid);
    } else if (!placement_updated && inserted && player.resolved_group_guid != 0) {
      if (HasWorkingSearchGroup(working_search_groups_, player.resolved_group_guid)) {
        InsertWorkingGroupMember(player.resolved_group_guid, guid, player.joined_group);
      } else {
        player.resolved_group_guid = 0;
        AppendWorkingStandalonePlayer(guid);
      }
    }
  }

  if (should_publish) {
    PublishSearchResults();
  }

  return reader.Good();
}

void LfgManager::QueueMissingSearchPlayerNameQueries(
    WorldSession *session, const std::function<void(std::uint64_t)> &send_name_query) {
  if (pending_search_player_name_query_requests_.empty()) {
    return;
  }

  for (const auto guid : pending_search_player_name_query_requests_) {
    if (guid == 0 || pending_search_player_name_guids_.contains(guid)) {
      continue;
    }
    if (ResolveSearchSnapshot(session, guid).has_value()) {
      continue;
    }

    pending_search_player_name_guids_.insert(guid);
    send_name_query(guid);
  }

  pending_search_player_name_query_requests_.clear();
}

bool LfgManager::ResolvePendingSearchPlayerNameQuery(const std::uint64_t guid) {
  if (pending_search_player_name_guids_.erase(guid) == 0) {
    return false;
  }

  return pending_search_player_name_guids_.empty();
}

void LfgManager::StartSearchBrowse(std::uint32_t packed_search_id) {
  if (joined_search_id_ == packed_search_id) {
    return;
  }

  update_lfg_list_blob_.clear();
  ResetSearchResultData();
  joined_search_id_ = packed_search_id;
}

void LfgManager::StopSearchBrowse() {
  if (joined_search_id_ == 0) {
    return;
  }

  update_lfg_list_blob_.clear();
  ResetSearchResultData();
  joined_search_id_ = 0;
}

void LfgManager::RefreshPublishedSearchResults() {
  PublishSearchResults();
}

std::uint32_t LfgManager::search_result_count() const {
  return static_cast<std::uint32_t>(published_group_order_.size() +
                                    published_standalone_player_order_.size());
}

std::uint32_t LfgManager::search_result_total_count() const {
  return reported_group_total_ + reported_player_total_;
}

std::vector<const LfgSearchGroupResult *> LfgManager::SearchGroups() const {
  std::vector<const LfgSearchGroupResult *> groups;
  groups.reserve(published_group_order_.size());
  for (const auto guid : published_group_order_) {
    if (const auto it = search_groups_.find(guid); it != search_groups_.end()) {
      groups.push_back(&it->second);
    }
  }
  return groups;
}

std::vector<const LfgSearchPlayerResult *> LfgManager::StandaloneSearchPlayers() const {
  std::vector<const LfgSearchPlayerResult *> players;
  players.reserve(published_standalone_player_order_.size());
  for (const auto guid : published_standalone_player_order_) {
    if (const auto it = search_players_.find(guid); it != search_players_.end()) {
      players.push_back(&it->second);
    }
  }
  return players;
}

const LfgSearchGroupResult *LfgManager::FindSearchGroup(std::uint64_t guid) const {
  const auto it = search_groups_.find(guid);
  return it != search_groups_.end() ? &it->second : nullptr;
}

const LfgSearchPlayerResult *LfgManager::FindSearchPlayer(std::uint64_t guid) const {
  const auto it = search_players_.find(guid);
  return it != search_players_.end() ? &it->second : nullptr;
}

const LfgSearchPlayerResult *LfgManager::GetSearchPrimaryPlayer(std::uint64_t group_guid) const {
  return GetSearchGroupMember(group_guid, 0);
}

const LfgSearchPlayerResult *LfgManager::GetSearchGroupMember(std::uint64_t group_guid,
                                                              std::size_t member_index) const {
  const auto *group = FindSearchGroup(group_guid);
  if (group == nullptr || member_index >= group->member_guids.size())
    return nullptr;

  return FindSearchPlayer(group->member_guids[member_index]);
}

void LfgManager::ToggleSearchGroupOrdering() {
  search_players_first_ = !search_players_first_;
}

void LfgManager::PromoteSearchSortKey(LfgSearchSortKey key) {
  auto it = std::find_if(search_sort_order_.begin(), search_sort_order_.end(),
                         [key](const auto &entry) { return entry.first == key; });
  if (it == search_sort_order_.end())
    return;

  auto entry = *it;
  if (it == search_sort_order_.begin()) {
    entry.second = !entry.second;
  }

  for (auto current = it; current != search_sort_order_.begin(); --current) {
    *current = *(current - 1);
  }
  search_sort_order_.front() = entry;
}

void LfgManager::ResortSearchResults(WorldSession *session,
                                     const openwow::data::dbc::DbcLoader *dbc) {
  SearchSortResolver resolver(session, dbc);

  const auto compare_search_players = [&](const std::uint64_t left_guid,
                                          const std::uint64_t right_guid) {
    const auto left_it = working_search_players_.find(left_guid);
    const auto right_it = working_search_players_.find(right_guid);
    if (left_it == working_search_players_.end())
      return right_it != working_search_players_.end();
    if (right_it == working_search_players_.end())
      return false;

    const auto &left = left_it->second;
    const auto &right = right_it->second;
    for (const auto &[key, descending] : search_sort_order_) {
      const int cmp = resolver.Compare(left, right, key, true);

      if (cmp != 0)
        return descending ? cmp > 0 : cmp < 0;
    }

    return false;
  };

  const auto compare_search_groups = [&](const std::uint64_t left_guid,
                                         const std::uint64_t right_guid) {
    const auto left_it = working_search_groups_.find(left_guid);
    const auto right_it = working_search_groups_.find(right_guid);
    if (left_it == working_search_groups_.end())
      return right_it != working_search_groups_.end();
    if (right_it == working_search_groups_.end())
      return false;

    const auto left_player_it =
        left_it->second.member_guids.empty()
            ? working_search_players_.end()
            : working_search_players_.find(left_it->second.member_guids.front());
    const auto right_player_it =
        right_it->second.member_guids.empty()
            ? working_search_players_.end()
            : working_search_players_.find(right_it->second.member_guids.front());
    if (left_player_it == working_search_players_.end())
      return right_player_it != working_search_players_.end();
    if (right_player_it == working_search_players_.end())
      return false;

    const auto &left = left_player_it->second;
    const auto &right = right_player_it->second;
    for (const auto &[key, descending] : search_sort_order_) {
      const int cmp = resolver.Compare(left, right, key, false);

      if (cmp != 0)
        return descending ? cmp > 0 : cmp < 0;
    }

    return false;
  };

  std::sort(working_group_order_.begin(), working_group_order_.end(), compare_search_groups);
  std::sort(working_standalone_player_order_.begin(), working_standalone_player_order_.end(),
            compare_search_players);
  PublishSearchResults();
}

void LfgManager::RemoveWorkingSearchResult(std::uint64_t guid) {
  if (working_search_groups_.contains(guid)) {
    RemoveWorkingGroup(guid);
    return;
  }

  RemoveWorkingPlayer(guid);
}

void LfgManager::RemoveWorkingGroup(std::uint64_t guid) {
  const auto it = working_search_groups_.find(guid);
  if (it == working_search_groups_.end())
    return;

  const auto member_guids = it->second.member_guids;
  for (const auto member_guid : member_guids) {
    RemoveWorkingPlayer(member_guid);
  }

  EraseGuid(working_group_order_, guid);
  working_search_groups_.erase(guid);
}

void LfgManager::RemoveWorkingPlayer(std::uint64_t guid) {
  const auto it = working_search_players_.find(guid);
  if (it == working_search_players_.end())
    return;

  if (it->second.resolved_group_guid != 0) {
    RemoveWorkingGroupMember(it->second.resolved_group_guid, guid);
  } else {
    RemoveWorkingStandalonePlayer(guid);
  }

  working_search_players_.erase(guid);
}

void LfgManager::RemoveWorkingStandalonePlayer(std::uint64_t guid) {
  EraseGuid(working_standalone_player_order_, guid);
}

void LfgManager::AppendWorkingStandalonePlayer(const std::uint64_t guid) {
  EraseGuid(working_standalone_player_order_, guid);
  working_standalone_player_order_.push_back(guid);
}

void LfgManager::RemoveWorkingGroupMember(std::uint64_t group_guid, std::uint64_t player_guid) {
  if (auto group_it = working_search_groups_.find(group_guid); group_it != working_search_groups_.end()) {
    EraseGuid(group_it->second.member_guids, player_guid);
  }
}

void LfgManager::InsertWorkingGroupMember(std::uint64_t group_guid, std::uint64_t player_guid,
                                          const bool joined_group) {
  const auto group_it = working_search_groups_.find(group_guid);
  if (group_it == working_search_groups_.end())
    return;

  auto &member_guids = group_it->second.member_guids;
  EraseGuid(member_guids, player_guid);
  if (joined_group) {
    member_guids.insert(member_guids.begin(), player_guid);
  } else {
    member_guids.push_back(player_guid);
  }
  EraseGuid(working_standalone_player_order_, player_guid);
}

void LfgManager::PublishSearchResults() {
  has_search_results_ = false;
  active_search_id_ = 0;
  reported_group_total_ = 0;
  reported_player_total_ = 0;
  search_groups_.clear();
  search_players_.clear();
  published_group_order_.clear();
  published_standalone_player_order_.clear();

  if (working_search_id_ == 0) {
    ++published_search_generation_;
    return;
  }

  has_search_results_ = true;
  active_search_id_ = working_search_id_;
  reported_group_total_ = working_reported_group_total_;
  reported_player_total_ = working_reported_player_total_;

  for (const auto guid : working_standalone_player_order_) {
    if (const auto it = working_search_players_.find(guid); it != working_search_players_.end()) {
      search_players_[guid] = it->second;
      published_standalone_player_order_.push_back(guid);
    }
  }

  for (const auto guid : working_group_order_) {
    const auto source_group_it = working_search_groups_.find(guid);
    if (source_group_it == working_search_groups_.end())
      continue;

    auto published_group = source_group_it->second;
    published_group.member_guids.clear();
    search_groups_[guid] = std::move(published_group);
    published_group_order_.push_back(guid);

    auto &target_group = search_groups_[guid];
    for (const auto member_guid : source_group_it->second.member_guids) {
      const auto source_player_it = working_search_players_.find(member_guid);
      if (source_player_it == working_search_players_.end())
        continue;

      search_players_[member_guid] = source_player_it->second;
      EraseGuid(published_standalone_player_order_, member_guid);
      if (source_player_it->second.joined_group) {
        target_group.member_guids.insert(target_group.member_guids.begin(), member_guid);
      } else {
        target_group.member_guids.push_back(member_guid);
      }
    }
  }

  ++published_search_generation_;
}

void LfgManager::ClearPendingSearchPlayerNameQueries() {
  pending_search_player_name_query_requests_.clear();
  pending_search_player_name_guids_.clear();
}

void LfgManager::ResetWorkingSearchResults() {
  ClearPendingSearchPlayerNameQueries();
  working_search_id_ = 0;
  working_reported_group_total_ = 0;
  working_reported_player_total_ = 0;
  working_search_groups_.clear();
  working_search_players_.clear();
  working_group_order_.clear();
  working_standalone_player_order_.clear();
}

void LfgManager::ResetPublishedSearchResults() {
  has_search_results_ = false;
  active_search_id_ = 0;
  reported_group_total_ = 0;
  reported_player_total_ = 0;
  search_groups_.clear();
  search_players_.clear();
  published_group_order_.clear();
  published_standalone_player_order_.clear();
  ++published_search_generation_;
}

void LfgManager::ResetSearchResultData() {
  ResetWorkingSearchResults();
  ResetPublishedSearchResults();
}

void LfgManager::ResetSearchOrdering() {
  search_players_first_ = true;
  search_sort_order_ = std::array<std::pair<LfgSearchSortKey, bool>, 7>{{
      {LfgSearchSortKey::kZone, false},
      {LfgSearchSortKey::kLevel, false},
      {LfgSearchSortKey::kClass, false},
      {LfgSearchSortKey::kName, false},
      {LfgSearchSortKey::kTank, false},
      {LfgSearchSortKey::kHealer, false},
      {LfgSearchSortKey::kDamage, false},
  }};
}

void LfgManager::ResetSearchResults() {
  ResetSearchResultData();
  ResetSearchOrdering();
}

}
