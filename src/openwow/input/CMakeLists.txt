openwow_add_component(openwow_input
  input_manager.cpp
  input_control.cpp
  hid_manager.cpp
)
target_link_libraries(openwow_input PRIVATE
  openwow_foundation
  openwow_lua
  SDL2::SDL2
)
