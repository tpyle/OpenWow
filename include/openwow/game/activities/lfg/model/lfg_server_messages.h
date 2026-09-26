#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

/// Decoded Dungeon Finder server messages (SMSG_LFG_*), as the client
/// stores them. Field meanings follow the 3.3.5a wire format; `raw_*`
/// fields are carried through without an established meaning.
namespace openwow::game {

enum class LfgUpdateType : std::uint8_t {
  kDefault = 0,
  kLeaderUnk1 = 1,
  kRolecheckAborted = 3,
  kJoinQueue = 4,
  kRolecheckFailed = 5,
  kRemovedFromQueue = 6,
  kProposalFailed = 7,
  kProposalDeclined = 8,
  kGroupFound = 9,
  kAddedToQueue = 10,
  kProposalBegin = 12,
  kUpdateStatus = 13,
  kGroupMemberOffline = 14,
  kGroupDisbandUnk16 = 15,
  kUnknown16 = 16,
};

struct LfgLockEntry {
  std::uint32_t dungeon_entry = 0;
  std::uint32_t lock_status = 0;
};

struct LfgPlayerLock {
  std::uint64_t player_guid = 0;
  std::vector<LfgLockEntry> locks;
};

struct LfgJoinResult {
  std::uint32_t result = 0;
  std::uint32_t state = 0;
  std::vector<LfgPlayerLock> player_locks;
};

struct LfgQueueStatus {
  std::uint32_t dungeon_id = 0;
  std::int32_t wait_time_avg = -1;
  std::int32_t wait_time = -1;
  std::int32_t wait_time_tank = -1;
  std::int32_t wait_time_healer = -1;
  std::int32_t wait_time_dps = -1;
  std::uint8_t tanks_needed = 0;
  std::uint8_t healers_needed = 0;
  std::uint8_t dps_needed = 0;
  std::uint32_t queued_time = 0;
};

struct LfgUpdateInfo {
  LfgUpdateType update_type = LfgUpdateType::kDefault;
  bool has_extra = false;
  bool joined = false;
  bool queued = false;
  bool raw_flag_4 = false;
  bool raw_flag_5 = false;
  std::array<std::uint8_t, 3> raw_tail_bytes{};
  std::vector<std::uint32_t> dungeons;
  std::string comment;

  /// Adds a packed dungeon id unless already present (keeps first-seen order).
  void AddDungeonSelection(std::uint32_t packed_dungeon_id);
  [[nodiscard]] bool ContainsDungeonSelection(std::uint32_t packed_dungeon_id) const;
  /// Whether `other` describes the same server state: flags, comment
  /// (case-sensitive) and the same set of dungeons in any order. The update
  /// type is not compared.
  [[nodiscard]] bool MatchesServerSnapshot(const LfgUpdateInfo& other) const;

  /// Copies everything but the update type from `src`, keeping the relative
  /// order of dungeons still selected and appending new ones.
  void SyncFrom(const LfgUpdateInfo& src);
};

struct LfgProposalPlayer {
  std::uint32_t role = 0;
  bool is_current_player = false;
  bool in_dungeon = false;
  bool same_group = false;
  bool has_answered = false;
  bool has_accepted = false;
};

struct LfgProposal {
  std::uint32_t dungeon_entry = 0;
  std::uint8_t state = 0;
  std::uint32_t proposal_id = 0;
  std::uint32_t encounter_mask = 0;
  bool silent = false;
  std::vector<LfgProposalPlayer> players;
};

struct LfgRoleCheckPlayer {
  std::uint64_t guid = 0;
  bool ready = false;
  std::uint32_t roles = 0;
  std::uint8_t level = 0;
};

struct LfgRoleCheckUpdate {
  std::uint32_t state = 0;
  bool is_beginning = false;
  std::vector<std::uint32_t> dungeons;
  std::vector<LfgRoleCheckPlayer> players;
};

struct LfgBootProposal {
  bool in_progress = false;
  bool did_vote = false;
  bool agree = false;
  std::uint64_t victim_guid = 0;
  std::uint32_t total_votes = 0;
  std::uint32_t agree_count = 0;
  std::uint32_t time_left = 0;
  std::uint32_t needed_votes = 0;
  std::string reason;
};

struct LfgRewardItem {
  std::uint32_t item_id = 0;
  std::uint32_t display_info_id = 0;
  std::uint32_t item_count = 0;
};

struct LfgPlayerReward {
  std::uint32_t random_dungeon_entry = 0;
  std::uint32_t completed_dungeon_entry = 0;
  bool is_first_reward = false;
  std::uint32_t strangers_count = 0;
  std::uint32_t base_money_reward = 0;
  std::uint32_t base_xp_reward = 0;
  std::uint32_t variable_money_reward = 0;
  std::uint32_t variable_xp_reward = 0;
  std::vector<LfgRewardItem> items;
};

struct LfgRoleChosen {
  std::uint64_t guid = 0;
  std::uint32_t roles = 0;
  std::uint8_t ready = 0;
};

struct LfgPlayerDungeonState {
  std::uint32_t packed_dungeon_id = 0;
  bool reward_done = false;
  std::uint32_t reward_money = 0;
  std::uint32_t reward_xp = 0;
  std::uint32_t reward_money_var = 0;
  std::uint32_t reward_xp_var = 0;
  std::vector<LfgRewardItem> rewards;
  bool locked = false;
  std::uint32_t lock_reason = 0;
};

struct LfgPartyLockInfo {
  std::uint64_t guid = 0;
  std::vector<LfgLockEntry> locks;
};

}  // namespace openwow::game
