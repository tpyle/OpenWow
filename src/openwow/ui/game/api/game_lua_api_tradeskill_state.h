#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct lua_State;

namespace openwow::data::dbc {
class DbcLoader;
}

namespace openwow::game {
class WorldSession;
}

namespace openwow::ui::game::detail {

bool HandleTradeSkillHyperlink(lua_State *L, const char *link);

[[nodiscard]] std::optional<std::string>
BuildTradeSkillListLink(lua_State *L, std::uint32_t trade_skill_spell_id);

void OpenTradeSkillView(openwow::game::WorldSession *session,
                        const openwow::data::dbc::DbcLoader *dbc,
                        std::uint32_t skill_line_id, std::string skill_name,
                        std::uint32_t current_rank, std::uint32_t max_rank,
                        std::optional<std::string> linked_player_name,
                        std::optional<std::string> encoded_recipe_bits = std::nullopt,
                        std::uint32_t trade_skill_spell_id = 0);

void CloseTradeSkillView(openwow::game::WorldSession *session = nullptr);

/// Refreshes the open window for the player's own profession after bags,
/// skills or known spells change: skill rank, each recipe's craftable count
/// and difficulty, and the recipe list itself when a recipe was learned.
/// Fires TRADE_SKILL_UPDATE. Does nothing for linked views or when closed.
void RefreshLocalTradeSkillView();
void HandleTradeSkillWorldLogout(openwow::game::WorldSession *session = nullptr);

}
