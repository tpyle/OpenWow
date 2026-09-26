#include "openwow/game/social/contacts/adapters/protocol/contact_server_packets.h"

#include "openwow/game/packet_reader.h"

namespace openwow::game::contacts {
namespace {

constexpr std::uint32_t kFriendFlag = 0x01u;
// guid + flags + an empty note's terminator.
constexpr std::size_t kMinimumContactWireSize = 13;

bool ReadFullPresence(PacketReader& reader, FriendPresence& presence) {
  return reader.ReadU8(presence.status) && reader.ReadU32(presence.area) &&
         reader.ReadU32(presence.level) && reader.ReadU32(presence.player_class);
}

}  // namespace

std::optional<ContactListMessage> DecodeContactList(const Payload payload) {
  PacketReader reader(payload);
  ContactListMessage message;
  std::uint32_t contact_count = 0;
  if (!reader.ReadU32(message.flags) || !reader.ReadU32(contact_count))
    return std::nullopt;
  // Reject counts the payload can't possibly hold before reserving.
  if (contact_count > reader.Remaining() / kMinimumContactWireSize)
    return std::nullopt;

  message.entries.reserve(contact_count);
  for (std::uint32_t i = 0; i < contact_count; ++i) {
    ContactListEntry entry;
    if (!reader.ReadU64(entry.guid) || !reader.ReadU32(entry.flags) ||
        !reader.ReadCString(entry.note))
      return std::nullopt;

    if ((entry.flags & kFriendFlag) != 0) {
      // Offline friends carry only the status byte.
      if (!reader.ReadU8(entry.presence.status))
        return std::nullopt;
      if (entry.presence.status != 0 &&
          (!reader.ReadU32(entry.presence.area) || !reader.ReadU32(entry.presence.level) ||
           !reader.ReadU32(entry.presence.player_class)))
        return std::nullopt;
    }
    message.entries.push_back(std::move(entry));
  }
  return message;
}

std::optional<FriendStatusMessage> DecodeFriendStatus(const Payload payload) {
  PacketReader reader(payload);
  FriendStatusMessage message;
  if (!reader.ReadU8(message.result) || !reader.ReadU64(message.guid))
    return std::nullopt;

  switch (message.result) {
  case 0x02:
    if (!ReadFullPresence(reader, message.presence))
      return std::nullopt;
    break;
  case 0x06:
  case 0x07:
    if (!reader.ReadCString(message.note))
      return std::nullopt;
    if (message.result == 0x06 && !ReadFullPresence(reader, message.presence))
      return std::nullopt;
    break;
  case 0x1A:
    if (!reader.ReadU8(message.presence.status))
      return std::nullopt;
    break;
  case 0x1B:
    if (!reader.ReadU32(message.presence.area))
      return std::nullopt;
    break;
  default:
    break;
  }
  return message;
}

}  // namespace openwow::game::contacts
