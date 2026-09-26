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

}  // namespace openwow::game::lfg
