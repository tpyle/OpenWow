
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "openwow/game/activities/lfg/model/lfg_server_messages.h"
#include "openwow/game/activities/lfg/rules/lfg_dungeon_rules.h"



namespace openwow::game {

namespace lfg {

/// What the client knows about a player in the browse list.
struct SearchPlayerIdentity {
  std::string name;
  std::uint8_t class_id = 0;
};

/// Lookups the browse-list sort needs from the rest of the client. A lookup
/// that finds nothing makes that sort key treat the pair as equal.
struct SearchSortContext {
  std::function<std::optional<SearchPlayerIdentity>(std::uint64_t guid)> find_player;
  /// AreaTable name; the view must stay valid for the sort and be followed by
  /// a NUL.
  std::function<std::optional<std::string_view>(std::uint32_t area_id)> area_name;
  /// ChrClasses name, same lifetime rule.
  std::function<std::optional<std::string_view>(std::uint8_t class_id)> class_name;
  /// Orders zone and class names (strcmp-style result).
  std::function<int(const char *, const char *)> compare_labels;
  /// Orders player names (strcmp-style result).
  std::function<int(const char *, const char *)> compare_player_names;
};

}  // namespace lfg

inline constexpr std::uint8_t kLfgRoleLeader = 1;
inline constexpr std::uint8_t kLfgRoleTank = 2;
inline constexpr std::uint8_t kLfgRoleHealer = 4;
inline constexpr std::uint8_t kLfgRoleDps = 8;

enum class LfgSearchSortKey : std::uint8_t {
  kZone = 0,
  kLevel = 1,
  kClass = 2,
  kName = 3,
  kTank = 4,
  kHealer = 5,
  kDamage = 6,
};

class LfgManager {
public:

  bool HandleLfgJoinResult(const std::uint8_t *data, std::size_t len);
  bool HandleLfgQueueStatus(const std::uint8_t *data, std::size_t len);
  bool HandleLfgUpdatePlayer(const std::uint8_t *data, std::size_t len);
  bool HandleLfgUpdateParty(const std::uint8_t *data, std::size_t len);
  bool HandleLfgProposalUpdate(const std::uint8_t *data, std::size_t len);
  bool HandleLfgRoleCheckUpdate(const std::uint8_t *data, std::size_t len);
  bool HandleLfgBootProposalUpdate(const std::uint8_t *data, std::size_t len);
  bool HandleLfgPlayerReward(const std::uint8_t *data, std::size_t len);
  bool HandleLfgTeleportDenied(const std::uint8_t *data, std::size_t len);
  bool HandleLfgOfferContinue(const std::uint8_t *data, std::size_t len);
  bool HandleLfgPlayerInfo(const std::uint8_t *data, std::size_t len);
  bool HandleLfgPartyInfo(const std::uint8_t *data, std::size_t len);
  bool HandleLfgRoleChosen(const std::uint8_t *data, std::size_t len);
  bool HandleLfgUpdateSearch(const std::uint8_t *data, std::size_t len);
  bool HandleLfgDisabled(const std::uint8_t *data, std::size_t len);
  bool HandleOpenLfgDungeonFinder(const std::uint8_t *data, std::size_t len);
  bool HandleUpdateLfgList(const std::uint8_t *data, std::size_t len);


  [[nodiscard]] const std::optional<LfgJoinResult> &join_result() const {
    return join_result_;
  }
  [[nodiscard]] const std::optional<LfgQueueStatus> &queue_status() const {
    return queue_status_;
  }
  [[nodiscard]] const std::optional<LfgUpdateInfo> &player_update() const {
    return player_update_;
  }
  [[nodiscard]] const std::optional<LfgUpdateInfo> &party_update() const {
    return party_update_;
  }
  [[nodiscard]] const std::optional<LfgProposal> &proposal() const {
    return proposal_;
  }
  [[nodiscard]] const std::optional<LfgRoleCheckUpdate> &role_check() const {
    return role_check_;
  }
  [[nodiscard]] const std::optional<LfgBootProposal> &boot_proposal() const {
    return boot_proposal_;
  }
  [[nodiscard]] const std::optional<LfgPlayerReward> &player_reward() const {
    return player_reward_;
  }
  [[nodiscard]] const LfgRewardItem *FindCompletionRewardItemByIndex(
      std::size_t reward_index) const;
  [[nodiscard]] std::uint32_t teleport_error() const {
    return teleport_error_;
  }
  [[nodiscard]] std::uint32_t offer_continue_dungeon() const {
    return offer_continue_dungeon_;
  }
  [[nodiscard]] const std::vector<std::uint8_t> &lfg_player_info_blob() const {
    return lfg_player_info_blob_;
  }
  [[nodiscard]] const std::vector<std::uint8_t> &lfg_party_info_blob() const {
    return lfg_party_info_blob_;
  }
  [[nodiscard]] const std::optional<LfgRoleChosen> &last_role_chosen() const {
    return last_role_chosen_;
  }
  [[nodiscard]] bool has_player_dungeon_info() const {
    return has_player_dungeon_info_;
  }
  [[nodiscard]] const LfgPlayerDungeonState *FindPlayerDungeonState(
      std::uint32_t packed_dungeon_id) const;
  [[nodiscard]] const LfgRewardItem *FindPlayerDungeonRewardItemByIndex(
      std::uint32_t packed_dungeon_id, std::size_t reward_index) const;
  [[nodiscard]] bool has_party_lock_info() const {
    return has_party_lock_info_;
  }
  [[nodiscard]] std::size_t GetLfdLockPlayerCount() const;
  [[nodiscard]] std::optional<std::uint32_t> FindPlayerLockReason(
      std::uint32_t dungeon_id) const;
  [[nodiscard]] const LfgPartyLockInfo *GetPartyLockInfo(std::size_t member_index) const;
  [[nodiscard]] const LfgPartyLockInfo *FindPartyLockByGuid(std::uint64_t guid) const;
  [[nodiscard]] std::optional<std::uint32_t> FindPartyLockReason(
      std::size_t member_index, std::uint32_t dungeon_id) const;
  [[nodiscard]] std::vector<LfgLockEntry> GetChoiceLockEntries() const;
  [[nodiscard]] bool HasUnlockedPlayerDungeon(std::uint32_t packed_dungeon_id) const;
  [[nodiscard]] bool IsDungeonJoinable(std::uint32_t packed_dungeon_id) const;
  /// See lfg::AvailableRandomDungeonIds; seasonal dungeons count when the
  /// player's dungeon info lists them unlocked.
  [[nodiscard]] std::vector<std::uint32_t> GetAvailableRandomDungeonIds(
      std::span<const lfg::RandomDungeonCandidate> candidates) const;
  /// See lfg::BestRandomDungeonId, with IsDungeonJoinable as the filter.
  [[nodiscard]] std::optional<std::uint32_t> GetBestRandomDungeonId(
      std::span<const lfg::RandomDungeonCandidate> candidates) const;
  [[nodiscard]] std::uint8_t lfg_update_search() const {
    return lfg_update_search_;
  }
  [[nodiscard]] bool lfg_disabled() const {
    return lfg_disabled_;
  }
  [[nodiscard]] std::uint32_t open_lfg_dungeon_id() const {
    return open_lfg_dungeon_id_;
  }
  [[nodiscard]] const std::vector<std::uint8_t> &update_lfg_list_blob() const {
    return update_lfg_list_blob_;
  }
  [[nodiscard]] bool has_search_results() const {
    return has_search_results_;
  }
  [[nodiscard]] std::uint32_t joined_search_id() const {
    return joined_search_id_;
  }
  [[nodiscard]] std::uint32_t active_search_id() const {
    return active_search_id_;
  }
  [[nodiscard]] std::uint32_t search_result_count() const;
  [[nodiscard]] std::uint32_t search_result_total_count() const;
  [[nodiscard]] bool search_players_first() const {
    return search_players_first_;
  }
  [[nodiscard]] const std::array<std::pair<LfgSearchSortKey, bool>, 7> &search_sort_order() const {
    return search_sort_order_;
  }
  [[nodiscard]] std::vector<const LfgSearchGroupResult *> SearchGroups() const;
  [[nodiscard]] std::vector<const LfgSearchPlayerResult *> StandaloneSearchPlayers() const;
  [[nodiscard]] const LfgSearchGroupResult *FindSearchGroup(std::uint64_t guid) const;
  [[nodiscard]] const LfgSearchPlayerResult *FindSearchPlayer(std::uint64_t guid) const;
  [[nodiscard]] const LfgSearchPlayerResult *GetSearchPrimaryPlayer(std::uint64_t group_guid) const;
  [[nodiscard]] const LfgSearchPlayerResult *GetSearchGroupMember(std::uint64_t group_guid,
                                                                  std::size_t member_index) const;
  void StartSearchBrowse(std::uint32_t packed_search_id);
  void StopSearchBrowse();
  void RefreshPublishedSearchResults();
  void ToggleSearchGroupOrdering();
  void PromoteSearchSortKey(LfgSearchSortKey key);
  /// Re-sorts the working browse list by `search_sort_order()`.
  void ResortSearchResults(const lfg::SearchSortContext &context);
  /// Sends a name query for each newly listed player the client doesn't
  /// know yet (at most one outstanding query per player).
  void QueueMissingSearchPlayerNameQueries(
      const std::function<bool(std::uint64_t)> &is_player_known,
      const std::function<void(std::uint64_t)> &send_name_query);
  void ApplyProposalResponse(bool accept);
  void ClearProposal();
  void ResetProposalEventGateForPlayerEnterWorld();
  [[nodiscard]] bool ConsumeProposalShowPending();
  [[nodiscard]] bool ResolvePendingSearchPlayerNameQuery(std::uint64_t guid);
  [[nodiscard]] std::uint64_t published_search_generation() const {
    return published_search_generation_;
  }

  void ClearServerInfoSnapshots();
  void ClearPlayerReward();
  void Clear();
  void ClearQueueStatus();

private:
  void RemoveWorkingSearchResult(std::uint64_t guid);
  void RemoveWorkingGroup(std::uint64_t guid);
  void RemoveWorkingPlayer(std::uint64_t guid);
  void RemoveWorkingStandalonePlayer(std::uint64_t guid);
  void AppendWorkingStandalonePlayer(std::uint64_t guid);
  void RemoveWorkingGroupMember(std::uint64_t group_guid, std::uint64_t player_guid);
  void InsertWorkingGroupMember(std::uint64_t group_guid, std::uint64_t player_guid,
                                bool joined_group);
  void PublishSearchResults();
  void ClearPendingSearchPlayerNameQueries();
  void ResetWorkingSearchResults();
  void ResetPublishedSearchResults();
  void ResetSearchResultData();
  void ResetSearchOrdering();
  void ResetSearchResults();

  std::optional<LfgJoinResult> join_result_;
  std::optional<LfgQueueStatus> queue_status_;
  std::optional<LfgUpdateInfo> player_update_;
  std::optional<LfgUpdateInfo> party_update_;
  std::optional<LfgProposal> proposal_;
  std::uint32_t last_proposal_event_id_ = 0;
  bool proposal_show_pending_ = false;
  std::optional<LfgRoleCheckUpdate> role_check_;
  std::optional<LfgBootProposal> boot_proposal_;
  std::optional<LfgPlayerReward> player_reward_;
  std::uint32_t teleport_error_ = 0;
  std::uint32_t offer_continue_dungeon_ = 0;
  std::vector<std::uint8_t> lfg_player_info_blob_;
  std::vector<std::uint8_t> lfg_party_info_blob_;
  std::optional<LfgRoleChosen> last_role_chosen_;
  bool has_player_dungeon_info_{false};
  bool has_party_lock_info_{false};
  std::vector<LfgPlayerDungeonState> player_dungeon_states_;
  std::vector<LfgPartyLockInfo> party_lock_info_;
  std::uint8_t lfg_update_search_{0};
  bool lfg_disabled_{false};
  std::uint32_t open_lfg_dungeon_id_{0};
  std::vector<std::uint8_t> update_lfg_list_blob_;
  std::uint32_t working_search_id_{0};
  std::uint32_t working_reported_group_total_{0};
  std::uint32_t working_reported_player_total_{0};
  std::vector<std::uint64_t> pending_search_player_name_query_requests_;
  std::unordered_set<std::uint64_t> pending_search_player_name_guids_;
  std::unordered_map<std::uint64_t, LfgSearchGroupResult> working_search_groups_;
  std::unordered_map<std::uint64_t, LfgSearchPlayerResult> working_search_players_;
  std::vector<std::uint64_t> working_group_order_;
  std::vector<std::uint64_t> working_standalone_player_order_;
  bool has_search_results_{false};
  std::uint32_t joined_search_id_{0};
  std::uint32_t active_search_id_{0};
  std::uint32_t reported_group_total_{0};
  std::uint32_t reported_player_total_{0};
  std::unordered_map<std::uint64_t, LfgSearchGroupResult> search_groups_;
  std::unordered_map<std::uint64_t, LfgSearchPlayerResult> search_players_;
  std::vector<std::uint64_t> published_group_order_;
  std::vector<std::uint64_t> published_standalone_player_order_;
  std::uint64_t published_search_generation_{0};
  bool search_players_first_{true};
  std::array<std::pair<LfgSearchSortKey, bool>, 7> search_sort_order_{{
      {LfgSearchSortKey::kZone, false},
      {LfgSearchSortKey::kLevel, false},
      {LfgSearchSortKey::kClass, false},
      {LfgSearchSortKey::kName, false},
      {LfgSearchSortKey::kTank, false},
      {LfgSearchSortKey::kHealer, false},
      {LfgSearchSortKey::kDamage, false},
  }};
};

}
