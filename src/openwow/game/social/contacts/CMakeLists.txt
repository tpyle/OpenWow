# Friends, ignore and mute lists. SocialManager and the Lua bindings still
# live in openwow_game / openwow_ui_runtime and move here in later steps.
# Nothing here may link openwow_game, openwow_engine_core or
# openwow_ui_runtime.

# Contact list types (header-only).
add_library(openwow_game_contacts_model INTERFACE)
target_include_directories(openwow_game_contacts_model
  INTERFACE ${PROJECT_SOURCE_DIR}/include
)

# Wire format of the contact packets.
add_library(openwow_game_contacts_protocol_adapter
  adapters/protocol/contact_client_packets.cpp
  adapters/protocol/contact_server_packets.cpp
)
# src/ is private: only for the header-only game/packet_reader.h.
target_include_directories(openwow_game_contacts_protocol_adapter
  PUBLIC ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(openwow_game_contacts_protocol_adapter
  PUBLIC openwow_network_protocol
)
openwow_configure_target(openwow_game_contacts_protocol_adapter)
