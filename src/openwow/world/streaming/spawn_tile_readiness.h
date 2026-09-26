#pragma once

namespace openwow::world {

/// Whether the terrain tile under a position is ready enough to place a unit
/// or resolve its area. Maps built around one global WMO have no terrain
/// tiles, and a tile the WDT doesn't list (open ocean, off-continent) will
/// never load, so neither is waited for; any other tile must be loaded.
[[nodiscard]] constexpr bool IsSpawnTileSettled(const bool has_global_wmo,
                                                const bool tile_exists_in_wdt,
                                                const bool tile_loaded) {
  return has_global_wmo || !tile_exists_in_wdt || tile_loaded;
}

}  // namespace openwow::world
