# Dungeon Finder (LFG). The protocol adapter (LfgManager) and the Lua
# bindings still live in openwow_game / openwow_ui_runtime and move here in
# later steps. Nothing here may link openwow_game, openwow_engine_core or
# openwow_ui_runtime.

add_library(openwow_game_lfg_rules
  rules/lfg_dungeon_rules.cpp
  rules/lfg_role_rules.cpp
)
target_include_directories(openwow_game_lfg_rules
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
openwow_configure_target(openwow_game_lfg_rules)

# Client-side LFG state (queue, proposal, boot vote, selection, throttles).
add_library(openwow_game_lfg_application
  application/lfg_system.cpp
)
target_include_directories(openwow_game_lfg_application
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(openwow_game_lfg_application
  PRIVATE openwow_game_lfg_rules
)
openwow_configure_target(openwow_game_lfg_application)

# Wire format of the LFG packets.
add_library(openwow_game_lfg_protocol_adapter
  adapters/protocol/lfg_client_packets.cpp
)
target_include_directories(openwow_game_lfg_protocol_adapter
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(openwow_game_lfg_protocol_adapter
  PUBLIC openwow_network_protocol
)
openwow_configure_target(openwow_game_lfg_protocol_adapter)
