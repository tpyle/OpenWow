# Friends, ignore and mute lists. The Lua bindings still live in
# openwow_ui_runtime.
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

# The client's contact lists (SocialManager).
add_library(openwow_game_contacts_application
  application/social_manager.cpp
)
target_include_directories(openwow_game_contacts_application
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(openwow_game_contacts_application
  PUBLIC openwow_game_contacts_model
  PRIVATE openwow_game_contacts_protocol_adapter
)
openwow_configure_target(openwow_game_contacts_application)
