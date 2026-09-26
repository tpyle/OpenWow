#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

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

/// An LFGDungeons row as the random-dungeon rules see it, with the
/// LFGDungeonExpansion override for the account's expansion already applied
/// to the level fields.
struct RandomDungeonCandidate {
  std::uint32_t dungeon_id = 0;
  std::uint32_t type_id = 0;
  std::uint32_t flags = 0;
  std::uint32_t expansion_level = 0;
  /// Midpoint of the recommended (or overridden target) level range.
  std::uint32_t target_level_average = 0;
  /// Minimum level (or the overridden hard minimum).
  std::uint32_t min_level = 0;
};

/// Type id of the "random dungeon" LFGDungeons rows.
inline constexpr std::uint32_t kRandomDungeonTypeId = 6u;
/// LFGDungeons flag: a seasonal dungeon offered in the random list while
/// unlocked.
inline constexpr std::uint32_t kSeasonalDungeonFlag = 0x4u;

/// Ids (unpacked) of the dungeons offered in the random-dungeon list, in
/// table order: every random dungeon, plus seasonal dungeons for which
/// `is_unlocked(packed id)` holds.
[[nodiscard]] std::vector<std::uint32_t> AvailableRandomDungeonIds(
    std::span<const RandomDungeonCandidate> candidates,
    const std::function<bool(std::uint32_t)>& is_unlocked);

/// The random dungeon to preselect: among joinable random dungeons
/// (`is_joinable(packed id)`), the newest expansion, then the highest
/// target level, then the highest minimum level; the first in table order
/// wins ties.
[[nodiscard]] std::optional<std::uint32_t> BestRandomDungeonId(
    std::span<const RandomDungeonCandidate> candidates,
    const std::function<bool(std::uint32_t)>& is_joinable);

}  // namespace openwow::game::lfg
