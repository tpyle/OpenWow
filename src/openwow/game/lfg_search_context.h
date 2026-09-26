#pragma once

#include "openwow/game/lfg_manager.h"

#include <cstdint>

namespace openwow::data::dbc {
class DbcLoader;
}

namespace openwow::game {

class WorldSession;

/// Builds the LFG browse-list sort lookups from the session's player cache
/// and the DBC tables. Either may be null (the matching sort keys then
/// compare equal). Zone and class names compare case-insensitively with the
/// client's collation; player names compare case-sensitively.
[[nodiscard]] lfg::SearchSortContext MakeLfgSearchSortContext(
    WorldSession *session, const openwow::data::dbc::DbcLoader *dbc);

/// Whether the client already has name and class data for a browse-list
/// player.
[[nodiscard]] bool IsLfgSearchPlayerKnown(WorldSession *session, std::uint64_t guid);

}  // namespace openwow::game
