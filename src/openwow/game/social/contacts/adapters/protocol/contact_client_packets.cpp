#include "openwow/game/social/contacts/adapters/protocol/contact_client_packets.h"

#include "openwow/network/protocol/wotlk/opcodes.h"

namespace openwow::game::contacts {

using net::wotlk::Opcode;
using net::wotlk::WorldPacket;

namespace {

// Appends `text` up to its first NUL, then the terminator.
void AppendCString(WorldPacket& pkt, const std::string_view text) {
  pkt.AppendString(text.substr(0, text.find('\0')));
}

WorldPacket GuidPacket(const Opcode opcode, const std::uint64_t guid) {
  WorldPacket pkt(opcode);
  pkt.AppendU64(guid);
  return pkt;
}

WorldPacket NamePacket(const Opcode opcode, const std::string_view name) {
  WorldPacket pkt(opcode);
  AppendCString(pkt, name);
  return pkt;
}

}  // namespace

WorldPacket BuildContactListRequestPacket(const std::uint32_t flags) {
  WorldPacket pkt(Opcode::CMSG_CONTACT_LIST);
  pkt.AppendU32(flags);
  return pkt;
}

WorldPacket BuildAddFriendPacket(const std::string_view name, const std::string_view note) {
  WorldPacket pkt(Opcode::CMSG_ADD_FRIEND);
  AppendCString(pkt, name);
  AppendCString(pkt, note);
  return pkt;
}

WorldPacket BuildDelFriendPacket(const std::uint64_t guid) {
  return GuidPacket(Opcode::CMSG_DEL_FRIEND, guid);
}

WorldPacket BuildSetContactNotesPacket(const std::uint64_t guid, const std::string_view note) {
  WorldPacket pkt(Opcode::CMSG_SET_CONTACT_NOTES);
  pkt.AppendU64(guid);
  AppendCString(pkt, note);
  return pkt;
}

WorldPacket BuildAddIgnorePacket(const std::string_view name) {
  return NamePacket(Opcode::CMSG_ADD_IGNORE, name);
}

WorldPacket BuildDelIgnorePacket(const std::uint64_t guid) {
  return GuidPacket(Opcode::CMSG_DEL_IGNORE, guid);
}

WorldPacket BuildAddMutePacket(const std::string_view name) {
  return NamePacket(Opcode::CMSG_ADD_VOICE_IGNORE, name);
}

WorldPacket BuildDelMutePacket(const std::uint64_t guid) {
  return GuidPacket(Opcode::CMSG_DEL_VOICE_IGNORE, guid);
}

}  // namespace openwow::game::contacts
