#include "openwow/data/blp/dxt_block_codec.h"

#include <catch2/catch_test_macros.hpp>

using openwow::data::blp::detail::BuildDxt5AlphaTable;
using openwow::data::blp::detail::ReadLittleEndian48;

TEST_CASE("ReadLittleEndian48 decodes 6 little-endian bytes into a 48-bit value",
          "[data][blp][dxt]") {
  // 0x123456789ABC, stored little-endian byte-by-byte.
  const std::uint8_t bytes[6] = {0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12};
  CHECK(ReadLittleEndian48(bytes) == 0x123456789ABCull);
}

TEST_CASE("ReadLittleEndian48 of all-zero bytes is zero", "[data][blp][dxt]") {
  const std::uint8_t bytes[6] = {0, 0, 0, 0, 0, 0};
  CHECK(ReadLittleEndian48(bytes) == 0ull);
}

TEST_CASE("BuildDxt5AlphaTable always places alpha0/alpha1 in slots 0/1", "[data][blp][dxt]") {
  const auto table = BuildDxt5AlphaTable(200, 50);
  CHECK(table[0] == 200);
  CHECK(table[1] == 50);
}

TEST_CASE("BuildDxt5AlphaTable uses 7-step interpolation when alpha0 > alpha1",
          "[data][blp][dxt]") {
  const auto table = BuildDxt5AlphaTable(255, 0);
  CHECK(table[2] == 219);
  CHECK(table[3] == 182);
  CHECK(table[4] == 146);
  CHECK(table[5] == 109);
  CHECK(table[6] == 73);
  CHECK(table[7] == 36);
}

TEST_CASE("BuildDxt5AlphaTable uses 5-step interpolation plus 0/255 anchors when "
          "alpha0 <= alpha1",
          "[data][blp][dxt]") {
  const auto table = BuildDxt5AlphaTable(0, 255);
  CHECK(table[2] == 51);
  CHECK(table[3] == 102);
  CHECK(table[4] == 153);
  CHECK(table[5] == 204);
  CHECK(table[6] == 0);
  CHECK(table[7] == 255);
}

TEST_CASE("BuildDxt5AlphaTable at alpha0 == alpha1 takes the 5-step branch (not > is false)",
          "[data][blp][dxt]") {
  const auto table = BuildDxt5AlphaTable(100, 100);
  CHECK(table[6] == 0);
  CHECK(table[7] == 255);
}
