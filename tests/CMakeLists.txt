find_package(Catch2 3 CONFIG REQUIRED)
find_package(Threads REQUIRED)
include(Catch)

add_executable(openwow_tests
  foundation/math_vec3_tests.cpp
  foundation/math_vec4_tests.cpp
  foundation/math_aabox_tests.cpp
  foundation/math_angle_tests.cpp
  foundation/math_scalar_tests.cpp
  foundation/math_ray_sphere_tests.cpp
  foundation/math_quadratic_roots_tests.cpp
  foundation/math_packed_mat3x3_tests.cpp
  foundation/math_row_major_mat4x4_tests.cpp
  foundation/math_packed_affine4x3_tests.cpp
  foundation/math_quaternion_xyzw_tests.cpp
  foundation/math_packed_quaternion64_tests.cpp
  foundation/math_fast_trig_approx_tests.cpp
  foundation/math_exp2_approx_tests.cpp
  foundation/math_cosine_ease_tests.cpp
  foundation/math_eval_poly_mag_sq_on_unit_circle_tests.cpp
  foundation/math_calc_swim_wave_bob_tests.cpp
  foundation/math_camera_matrix_tests.cpp
  foundation/memory_frame_arena_tests.cpp
  foundation/threading_recursive_mutex_tests.cpp
  foundation/math_loose_octree_tests.cpp
  foundation/hashing_tests.cpp
  foundation/text_ascii_tests.cpp
  foundation/text_utf8_tests.cpp
  foundation/container_flat_hash_map_tests.cpp
  network/byte_buffer_tests.cpp
  network/zlib_compression_tests.cpp
  net/inline_download_file_name_tests.cpp
  net/realm_type_tests.cpp
  runtime/frame_scheduler_tests.cpp
  data/dbc_file_tests.cpp
  data/faction_reaction_tests.cpp
  data/binary_reader_tests.cpp
  data/wdt_file_tests.cpp
  data/blp_info_tests.cpp
  data/dxt_block_codec_tests.cpp
  data/wdb_cache_directory_tests.cpp
  # wdt_file.cpp/blp_info.cpp/faction_reaction.cpp are compiled directly
  # here (rather than by linking the full openwow_data_runtime target)
  # because they have no dependency beyond header-only siblings
  # (binary_reader.h, wow_chunk_fourcc.h, dbc_store.h, unit_defines.h) --
  # linking openwow_data_runtime would pull in
  # StormLib/Lua/platform_filesystem/JPEG for no reason. frame_scheduler.cpp
  # is likewise compiled directly instead of linking all of
  # openwow_engine_core.
  ${PROJECT_SOURCE_DIR}/src/openwow/data/terrain/wdt_file.cpp
  ${PROJECT_SOURCE_DIR}/src/openwow/data/blp/blp_info.cpp
  ${PROJECT_SOURCE_DIR}/src/openwow/data/formats/dbc/faction_reaction.cpp
  ${PROJECT_SOURCE_DIR}/src/openwow/runtime/scheduling/frame_scheduler.cpp
  ${PROJECT_SOURCE_DIR}/src/openwow/data/wdb_persistence.cpp
  ${PROJECT_SOURCE_DIR}/src/openwow/data/wdb_cache.cpp
  ${PROJECT_SOURCE_DIR}/src/openwow/net/wotlk/realm_list.cpp
)

target_include_directories(openwow_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(openwow_tests PRIVATE
  Catch2::Catch2WithMain
  openwow_foundation
  openwow_foundation_text
  openwow_network_serialization
  openwow_data_formats
  Threads::Threads
)

openwow_configure_target(openwow_tests)

catch_discover_tests(openwow_tests)
