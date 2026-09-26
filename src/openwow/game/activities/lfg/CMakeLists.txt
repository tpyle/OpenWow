# Dungeon Finder (LFG). The protocol adapter (LfgManager) and the Lua
# bindings still live in openwow_game / openwow_ui_runtime and move here in
# later steps. Nothing here may link openwow_game, openwow_engine_core or
# openwow_ui_runtime.

# Decoded server messages (SMSG_LFG_*).
add_library(openwow_game_lfg_model
  model/lfg_server_messages.cpp
)
target_include_directories(openwow_game_lfg_model
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
openwow_configure_target(openwow_game_lfg_model)

add_library(openwow_game_lfg_rules
  rules/lfg_dungeon_rules.cpp
  rules/lfg_role_rules.cpp
)
target_include_directories(openwow_game_lfg_rules
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
openwow_configure_target(openwow_game_lfg_rules)

# Client-side LFG state: LFGSystem (queue, proposal, boot vote, selection,
# throttles) and LfgManager (server-reported state, locks, browse list).
# LfgManager's handlers take raw SMSG payloads, so it uses the protocol
# adapter's decoders; the targets below it stay free of the session.
add_library(openwow_game_lfg_application
  application/lfg_manager.cpp
  application/lfg_system.cpp
)
target_include_directories(openwow_game_lfg_application
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(openwow_game_lfg_application
  PUBLIC
    openwow_game_lfg_model
    openwow_game_lfg_rules
  PRIVATE
    openwow_game_lfg_protocol_adapter
)
openwow_configure_target(openwow_game_lfg_application)

# Wire format of the LFG packets.
add_library(openwow_game_lfg_protocol_adapter
  adapters/protocol/lfg_client_packets.cpp
  adapters/protocol/lfg_server_packets.cpp
)
# src/ is private: only for the header-only game/packet_reader.h.
target_include_directories(openwow_game_lfg_protocol_adapter
  PUBLIC ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(openwow_game_lfg_protocol_adapter
  PUBLIC
    openwow_game_lfg_model
    openwow_network_protocol
)
openwow_configure_target(openwow_game_lfg_protocol_adapter)

# LFG data read from the DBC tables.
add_library(openwow_game_lfg_dbc_adapter
  adapters/data/lfg_dbc_dungeons.cpp
)
target_include_directories(openwow_game_lfg_dbc_adapter
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(openwow_game_lfg_dbc_adapter
  PUBLIC openwow_game_lfg_rules
  PRIVATE
    openwow_data_formats
    openwow_data_runtime
)
openwow_configure_target(openwow_game_lfg_dbc_adapter)
