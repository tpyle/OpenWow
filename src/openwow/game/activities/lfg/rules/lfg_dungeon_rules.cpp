#include "openwow/game/activities/lfg/rules/lfg_dungeon_rules.h"

namespace openwow::game::lfg {

bool CanKeepSelectedDungeon(const std::uint8_t new_type,
                            const std::uint32_t tracked_party_member_count,
                            const std::uint32_t existing_packed_dungeon_id) {
  const auto existing_type = DungeonSelectionType(existing_packed_dungeon_id);
  if (new_type == 1u || new_type == 5u) {
    return existing_type == 1u || existing_type == 5u;
  }

  if (new_type == 2u && tracked_party_member_count == 0u) {
    return existing_type == 2u;
  }

  return false;
}

std::vector<std::uint32_t> AvailableRandomDungeonIds(
    const std::span<const RandomDungeonCandidate> candidates,
    const std::function<bool(std::uint32_t)>& is_unlocked) {
  std::vector<std::uint32_t> dungeon_ids;
  for (const auto& candidate : candidates) {
    if (candidate.type_id == kRandomDungeonTypeId ||
        ((candidate.flags & kSeasonalDungeonFlag) != 0 &&
         is_unlocked(PackDungeonId(candidate.dungeon_id, candidate.type_id)))) {
      dungeon_ids.push_back(candidate.dungeon_id);
    }
  }
  return dungeon_ids;
}

std::optional<std::uint32_t> BestRandomDungeonId(
    const std::span<const RandomDungeonCandidate> candidates,
    const std::function<bool(std::uint32_t)>& is_joinable) {
  const RandomDungeonCandidate* best = nullptr;
  for (const auto& candidate : candidates) {
    if (candidate.type_id != kRandomDungeonTypeId ||
        !is_joinable(PackDungeonId(candidate.dungeon_id, candidate.type_id))) {
      continue;
    }
    if (best == nullptr) {
      best = &candidate;
      continue;
    }
    if (candidate.expansion_level != best->expansion_level) {
      if (candidate.expansion_level > best->expansion_level) {
        best = &candidate;
      }
      continue;
    }
    if (candidate.target_level_average != best->target_level_average) {
      if (candidate.target_level_average > best->target_level_average) {
        best = &candidate;
      }
      continue;
    }
    if (candidate.min_level > best->min_level) {
      best = &candidate;
    }
  }

  if (best == nullptr) {
    return std::nullopt;
  }
  return best->dungeon_id;
}

}  // namespace openwow::game::lfg
