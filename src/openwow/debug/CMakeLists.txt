find_package(boost_asio CONFIG REQUIRED)
find_package(boost_system CONFIG REQUIRED)
find_package(boost_json CONFIG REQUIRED)
find_package(Threads REQUIRED)

openwow_add_component(openwow_debug_runtime
  control/debug_control_server.cpp
  control/debug_control_json_codec.cpp
  diagnostics/debug_console.cpp
  diagnostics/error_handler.cpp
  capture/frame_replay_archive.cpp
  diagnostics/profiler.cpp
  inspection/render_debug.cpp
)
target_link_libraries(openwow_debug_runtime PRIVATE
  openwow_foundation
  openwow_foundation_diagnostics
  openwow_render_api
  Boost::asio
  Boost::system
  Boost::json
  OpenSSL::Crypto
  Threads::Threads
  ZLIB::ZLIB
)
