# Dungeon Finder (LFG). Pure rules first; the application state, protocol
# adapter and Lua bindings still live in openwow_game / openwow_ui_runtime
# and move here in later steps.

add_library(openwow_game_lfg_rules
  rules/lfg_dungeon_rules.cpp
  rules/lfg_role_rules.cpp
)
target_include_directories(openwow_game_lfg_rules
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
openwow_configure_target(openwow_game_lfg_rules)
