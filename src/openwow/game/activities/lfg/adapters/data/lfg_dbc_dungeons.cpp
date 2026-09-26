#include "openwow/game/activities/lfg/adapters/data/lfg_dbc_dungeons.h"

#include "openwow/data/formats/dbc/dbc_loader.h"
#include "openwow/data/formats/dbc/dbc_table_registry.h"

namespace openwow::game::lfg {

std::vector<RandomDungeonCandidate> BuildRandomDungeonCandidates(
    const openwow::data::dbc::DbcLoader& dbc, const std::uint8_t expansion_level) {
  std::vector<RandomDungeonCandidate> candidates;
  for (const auto& entry : dbc.lfg_dungeons()) {
    const auto* override_entry =
        openwow::data::DBClient_FindLfgDungeonExpansion(&dbc, entry.id, expansion_level);
    const auto target_min =
        override_entry != nullptr ? override_entry->target_level_min : entry.rec_min_level;
    const auto target_max =
        override_entry != nullptr ? override_entry->target_level_max : entry.rec_max_level;
    candidates.push_back({
        .dungeon_id = entry.id,
        .type_id = entry.type_id,
        .flags = entry.flags,
        .expansion_level = entry.expansion_level,
        .target_level_average = (target_min + target_max) / 2,
        .min_level = override_entry != nullptr ? override_entry->hard_level_min : entry.min_level,
    });
  }
  return candidates;
}

}  // namespace openwow::game::lfg
