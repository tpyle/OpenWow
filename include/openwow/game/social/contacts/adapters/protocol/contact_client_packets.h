#pragma once

#include "openwow/network/protocol/wotlk/world_packet.h"

#include <cstdint>
#include <string_view>

/// Builders for the friends, ignore and mute (voice ignore) packets the
/// client sends. Names and notes are sent as NUL-terminated strings and end
/// at their first NUL character.
namespace openwow::game::contacts {

/// CMSG_CONTACT_LIST: asks for the lists selected by `flags` (SocialFlag bits).
[[nodiscard]] net::wotlk::WorldPacket BuildContactListRequestPacket(std::uint32_t flags);

/// CMSG_ADD_FRIEND.
[[nodiscard]] net::wotlk::WorldPacket BuildAddFriendPacket(std::string_view name,
                                                           std::string_view note = {});
/// CMSG_DEL_FRIEND.
[[nodiscard]] net::wotlk::WorldPacket BuildDelFriendPacket(std::uint64_t guid);
/// CMSG_SET_CONTACT_NOTES.
[[nodiscard]] net::wotlk::WorldPacket BuildSetContactNotesPacket(std::uint64_t guid,
                                                                 std::string_view note);

/// CMSG_ADD_IGNORE.
[[nodiscard]] net::wotlk::WorldPacket BuildAddIgnorePacket(std::string_view name);
/// CMSG_DEL_IGNORE.
[[nodiscard]] net::wotlk::WorldPacket BuildDelIgnorePacket(std::uint64_t guid);

/// CMSG_ADD_VOICE_IGNORE (mute).
[[nodiscard]] net::wotlk::WorldPacket BuildAddMutePacket(std::string_view name);
/// CMSG_DEL_VOICE_IGNORE (unmute).
[[nodiscard]] net::wotlk::WorldPacket BuildDelMutePacket(std::uint64_t guid);

}  // namespace openwow::game::contacts
