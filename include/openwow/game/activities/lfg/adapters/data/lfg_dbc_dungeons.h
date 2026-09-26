#pragma once

#include "openwow/game/activities/lfg/rules/lfg_dungeon_rules.h"

#include <cstdint>
#include <vector>

namespace openwow::data::dbc {
class DbcLoader;
}

/// Dungeon Finder data read from the client's DBC tables.
namespace openwow::game::lfg {

/// Every LFGDungeons row as a random-dungeon candidate, in table order, with
/// the LFGDungeonExpansion override for `expansion_level` applied.
[[nodiscard]] std::vector<RandomDungeonCandidate> BuildRandomDungeonCandidates(
    const openwow::data::dbc::DbcLoader& dbc, std::uint8_t expansion_level);

}  // namespace openwow::game::lfg
