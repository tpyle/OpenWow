#pragma once

#include "openwow/game/object_guid.h"

#include <cstdint>
#include <string>

/// Friends, ignore and mute (voice ignore) list types.
namespace openwow::game {

enum class FriendStatus : std::uint8_t {
  kOffline = 0x00,
  kOnline = 0x01,
  kAfk = 0x02,
  kDnd = 0x04,
  kRaf = 0x08,
};

enum class SocialFlag : std::uint32_t {
  kNone = 0x00,
  kFriend = 0x01,
  kIgnored = 0x02,
  kMuted = 0x04,
};

inline SocialFlag operator|(SocialFlag a, SocialFlag b) {
  return static_cast<SocialFlag>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
inline bool HasSocialFlag(SocialFlag field, SocialFlag flag) {
  return (static_cast<std::uint32_t>(field) & static_cast<std::uint32_t>(flag)) != 0;
}

enum class FriendsResult : std::uint8_t {
  kDbError = 0x00,
  kListFull = 0x01,
  kOnline = 0x02,
  kOffline = 0x03,
  kNotFound = 0x04,
  kRemoved = 0x05,
  kAddedOnline = 0x06,
  kAddedOffline = 0x07,
  kAlready = 0x08,
  kSelf = 0x09,
  kEnemy = 0x0A,
  kIgnoreListFull = 0x0B,
  kIgnoreSelf = 0x0C,
  kIgnoreNotFound = 0x0D,
  kIgnoreAlready = 0x0E,
  kIgnoreAdded = 0x0F,
  kIgnoreRemoved = 0x10,
  kMuteSelf = 0x11,
  kMuteNotFound = 0x12,
  kMuteAlready = 0x13,
  kMuteAdded = 0x15,
  kMuteAddedOnline = 0x16,
  kMuteRemoved = 0x17,
  kMuteListFull = 0x18,
  kMuteAmbiguous = 0x19,
  kFriendAlreadyOnline = 0x1A,
  kFriendAreaChanged = 0x1B,
};

struct ContactInfo {
  ObjectGuid guid;
  SocialFlag flags{SocialFlag::kNone};
  std::string note;
  std::string display_name;
  FriendStatus status{FriendStatus::kOffline};
  std::uint32_t area{0};
  std::uint32_t level{0};
  std::uint32_t player_class{0};
  bool friend_delete_pending{false};
  bool ignore_delete_pending{false};
  bool mute_delete_pending{false};
};

struct FriendStatusUpdate {
  FriendsResult result{FriendsResult::kDbError};
  ObjectGuid guid;
};

}  // namespace openwow::game
