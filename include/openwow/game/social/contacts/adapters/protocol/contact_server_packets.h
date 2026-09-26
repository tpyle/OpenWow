#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// Decoders for the contact packets the server sends. Each returns nothing
/// when the payload is too short or malformed; trailing bytes are ignored.
namespace openwow::game::contacts {

using Payload = std::span<const std::uint8_t>;

/// A friend's presence as reported by the server. `area`, `level` and
/// `player_class` are zero when the friend is offline.
struct FriendPresence {
  std::uint8_t status = 0;
  std::uint32_t area = 0;
  std::uint32_t level = 0;
  std::uint32_t player_class = 0;
};

/// One SMSG_CONTACT_LIST entry. `presence` is read only for entries with the
/// friend flag.
struct ContactListEntry {
  std::uint64_t guid = 0;
  std::uint32_t flags = 0;
  std::string note;
  FriendPresence presence;
};

/// SMSG_CONTACT_LIST: the lists selected by `flags` (SocialFlag bits),
/// replacing what the client had for those lists.
struct ContactListMessage {
  std::uint32_t flags = 0;
  std::vector<ContactListEntry> entries;
};
[[nodiscard]] std::optional<ContactListMessage> DecodeContactList(Payload payload);

/// SMSG_FRIEND_STATUS. Which optional fields are present depends on
/// `result` (FriendsResult): 0x02 (online) and 0x06 (added, online) carry
/// presence, 0x06 and 0x07 (added) carry the note, 0x1A a new status byte
/// (in `presence.status`) and 0x1B a new area (in `presence.area`).
struct FriendStatusMessage {
  std::uint8_t result = 0;
  std::uint64_t guid = 0;
  std::string note;
  FriendPresence presence;
};
[[nodiscard]] std::optional<FriendStatusMessage> DecodeFriendStatus(Payload payload);

}  // namespace openwow::game::contacts
