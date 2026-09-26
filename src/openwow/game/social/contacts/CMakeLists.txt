# Friends, ignore and mute lists. SocialManager and the Lua bindings still
# live in openwow_game / openwow_ui_runtime and move here in later steps.
# Nothing here may link openwow_game, openwow_engine_core or
# openwow_ui_runtime.

# Wire format of the contact packets.
add_library(openwow_game_contacts_protocol_adapter
  adapters/protocol/contact_client_packets.cpp
)
target_include_directories(openwow_game_contacts_protocol_adapter
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(openwow_game_contacts_protocol_adapter
  PUBLIC openwow_network_protocol
)
openwow_configure_target(openwow_game_contacts_protocol_adapter)
