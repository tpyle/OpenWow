#pragma once

#include <cstdint>

/// Dungeon Finder dungeon ids and selection rules.
///
/// The client and server exchange "packed" dungeon ids: the LFGDungeons.dbc
/// row id in the low 24 bits and the dungeon's type (LFGDungeons TypeID:
/// 1 dungeon, 2 raid, 5 heroic, 6 random, ...) in the high 8 bits.
namespace openwow::game::lfg {

/// Packs an LFGDungeons row id and its type id.
[[nodiscard]] constexpr std::uint32_t PackDungeonId(
    const std::uint32_t dungeon_id, const std::uint32_t type_id) noexcept {
  return (dungeon_id & 0x00FFFFFFu) | (type_id << 24);
}

/// The type id of a packed dungeon id.
[[nodiscard]] constexpr std::uint8_t DungeonSelectionType(
    const std::uint32_t packed_dungeon_id) noexcept {
  return static_cast<std::uint8_t>(packed_dungeon_id >> 24);
}

/// Whether an already selected dungeon (`existing_packed_dungeon_id`) may
/// stay selected when a dungeon of type `new_type` is added. Dungeons and
/// heroics (types 1 and 5) mix with each other; raids (type 2) mix only with
/// raids and only when the player isn't in a party; any other addition
/// replaces the selection.
[[nodiscard]] bool CanKeepSelectedDungeon(std::uint8_t new_type,
                                          std::uint32_t tracked_party_member_count,
                                          std::uint32_t existing_packed_dungeon_id);

}  // namespace openwow::game::lfg
