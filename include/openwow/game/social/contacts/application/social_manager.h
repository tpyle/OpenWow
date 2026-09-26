#pragma once

#include "openwow/game/object_guid.h"
#include "openwow/game/social/contacts/model/contact_types.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace openwow::game {

/// The client's friends, ignore and mute lists as the server reports them,
/// plus the UI's selection in each list and recently reported players.
/// Lists are ordered for display: friends online first, then by name;
/// ignored and muted players by name; unnamed contacts last.
class SocialManager {
public:
  static constexpr std::size_t kFriendLimit = 100;
  static constexpr std::size_t kIgnoreLimit = 50;
  static constexpr std::size_t kMuteLimit = 50;
  static constexpr std::size_t kNoteMaxLen = 48;
  static constexpr std::size_t kRecentComplaintLimit = 32;
  static constexpr std::uint8_t kComplaintStatusEnabled = 2;

  /// Orders contact names (strcmp-style result); the lists sort by it.
  using NameOrder = std::function<int(const char *, const char *)>;

  explicit SocialManager(NameOrder name_order);

  bool HandleContactList(const std::uint8_t *data, std::size_t len);

  bool HandleFriendStatus(const std::uint8_t *data, std::size_t len);

  [[nodiscard]] const std::vector<ContactInfo> &contacts() const {
    return contacts_;
  }
  [[nodiscard]] std::size_t contact_count() const {
    return contacts_.size();
  }

  [[nodiscard]] std::vector<const ContactInfo *> GetFriends() const;
  [[nodiscard]] std::vector<const ContactInfo *> GetIgnored() const;
  [[nodiscard]] std::vector<const ContactInfo *> GetMuted() const;

  void SelectFriendByLuaIndex(std::uint32_t index);
  void SelectIgnoredByLuaIndex(std::uint32_t index);
  void SelectMutedByLuaIndex(std::uint32_t index);
  [[nodiscard]] std::int32_t GetSelectedFriendLuaIndex() const;
  [[nodiscard]] std::int32_t GetSelectedIgnoredLuaIndex() const;
  [[nodiscard]] std::int32_t GetSelectedMutedLuaIndex() const;
  void SetWhoResultsToUi(bool enabled) { who_results_to_ui_ = enabled; }
  [[nodiscard]] bool WhoResultsToUi() const { return who_results_to_ui_; }

  [[nodiscard]] const ContactInfo *FindContact(const ObjectGuid &guid) const;
  [[nodiscard]] bool HasContact(const ObjectGuid &guid) const;
  [[nodiscard]] bool IsFriend(const ObjectGuid &guid) const;
  [[nodiscard]] bool IsIgnored(const ObjectGuid &guid) const;
  [[nodiscard]] bool IsMuted(const ObjectGuid &guid) const;
  [[nodiscard]] bool IsIgnoredOrMuted(const ObjectGuid &guid) const;
  [[nodiscard]] bool IsDeletePending(const ObjectGuid &guid, SocialFlag flag) const;
  [[nodiscard]] bool HasRecentComplaintGuid(std::uint64_t guid) const;
  [[nodiscard]] const std::optional<FriendStatusUpdate> &last_friend_status_update() const {
    return last_friend_status_update_;
  }

  bool SetDisplayName(const ObjectGuid &guid, std::string display_name);
  bool MarkDeletePending(const ObjectGuid &guid, SocialFlag flag);
  bool ClearFriendReferAFriendFlag(const ObjectGuid &guid);
  void RememberRecentComplaintGuid(std::uint64_t guid);
  void SetComplaintStatus(std::uint8_t status) { complaint_status_ = status; }
  [[nodiscard]] std::uint8_t complaint_status() const { return complaint_status_; }
  void Clear();

private:
  std::vector<ContactInfo> contacts_;
  std::vector<std::uint64_t> recent_complaint_guids_;
  std::optional<FriendStatusUpdate> last_friend_status_update_;
  std::uint8_t complaint_status_{0};
  ObjectGuid selected_friend_;
  ObjectGuid selected_ignored_;
  ObjectGuid selected_muted_;
  bool who_results_to_ui_{false};
  NameOrder name_order_;

  ContactInfo *FindContactMut(const ObjectGuid &guid);
  ContactInfo &GetOrCreateContact(const ObjectGuid &guid);
  void ClearRequestedFlags(std::uint32_t requested_flags);
  static bool IsVisible(const ContactInfo &contact, SocialFlag flag);
  bool NameComesBefore(const std::string &lhs, const std::string &rhs) const;
  static ObjectGuid GuidAtLuaIndex(
      const std::vector<const ContactInfo *> &contacts, std::uint32_t index);
  static std::int32_t LuaIndexOfGuid(
      const std::vector<const ContactInfo *> &contacts, ObjectGuid guid);
};

}
