#include "openwow/data/model/binary_reader.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

using Catch::Approx;
using openwow::data::model::BinaryReader;

TEST_CASE("BinaryReader::CanRead is false for a default-constructed (null) reader",
          "[data][binary_reader]") {
  BinaryReader reader;
  CHECK_FALSE(reader.CanRead(0, 0));
  CHECK_FALSE(reader.CanRead(0, 1));
}

TEST_CASE("BinaryReader::CanRead accepts a zero-byte read exactly at the end of the buffer, "
          "but rejects any nonzero read there",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {1, 2, 3, 4};
  BinaryReader reader(bytes.data(), bytes.size());
  CHECK(reader.CanRead(4, 0));
  CHECK_FALSE(reader.CanRead(4, 1));
  CHECK_FALSE(reader.CanRead(5, 0)); // offset past the end is always rejected
}

TEST_CASE("BinaryReader::ReadU8/ReadU16/ReadU32 decode little-endian and reject "
          "out-of-range reads",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {0x11, 0x22, 0x33, 0x44};
  BinaryReader reader(bytes.data(), bytes.size());

  REQUIRE(reader.ReadU8(0).has_value());
  CHECK(*reader.ReadU8(0) == 0x11);

  REQUIRE(reader.ReadU16(0).has_value());
  CHECK(*reader.ReadU16(0) == 0x2211);

  REQUIRE(reader.ReadU32(0).has_value());
  CHECK(*reader.ReadU32(0) == 0x44332211u);

  CHECK_FALSE(reader.ReadU32(1).has_value()); // 1 + 4 > 4 bytes total
  CHECK_FALSE(reader.ReadU8(4).has_value());  // exactly past the end
}

TEST_CASE("BinaryReader::ReadI16/ReadI32 reinterpret the same bits as ReadU16/ReadU32",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {0xFF, 0xFF, 0xFF, 0xFF};
  BinaryReader reader(bytes.data(), bytes.size());
  CHECK(*reader.ReadI16(0) == -1);
  CHECK(*reader.ReadI32(0) == -1);
}

TEST_CASE("BinaryReader::ReadF32 decodes a little-endian IEEE-754 float", "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {0x00, 0x00, 0xC0, 0x3F}; // 1.5f
  BinaryReader reader(bytes.data(), bytes.size());
  REQUIRE(reader.ReadF32(0).has_value());
  CHECK(*reader.ReadF32(0) == Approx(1.5f));
}

TEST_CASE("BinaryReader::ReadBytes returns a span over the requested range",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {1, 2, 3, 4, 5};
  BinaryReader reader(bytes.data(), bytes.size());
  const auto span = reader.ReadBytes(1, 3);
  REQUIRE(span.has_value());
  CHECK(span->size() == 3);
  CHECK((*span)[0] == 2);
  CHECK((*span)[2] == 4);
  CHECK_FALSE(reader.ReadBytes(1, 5).has_value()); // 1 + 5 > 5 bytes total
}

TEST_CASE("BinaryReader::ReadCString stops at the first embedded null within max_bytes",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {'h', 'i', '\0', 'X', 'X'};
  BinaryReader reader(bytes.data(), bytes.size());
  REQUIRE(reader.ReadCString(0, 5).has_value());
  CHECK(*reader.ReadCString(0, 5) == "hi");
}

TEST_CASE("BinaryReader::ReadCString returns the available bytes, un-truncated by a null, "
          "when none is found within range",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {'a', 'b', 'c'}; // no null terminator anywhere
  BinaryReader reader(bytes.data(), bytes.size());
  REQUIRE(reader.ReadCString(0, 10).has_value()); // max_bytes exceeds the buffer
  CHECK(*reader.ReadCString(0, 10) == "abc");
}

TEST_CASE("BinaryReader::ReadCString fails only when the starting offset itself is unreadable",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {'a', 'b'};
  BinaryReader reader(bytes.data(), bytes.size());
  CHECK_FALSE(reader.ReadCString(2, 5).has_value()); // offset == size, no bytes available
}

TEST_CASE("BinaryReader::ReadString returns exactly the requested byte range as a string",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {'h', 'e', 'l', 'l', 'o'};
  BinaryReader reader(bytes.data(), bytes.size());
  REQUIRE(reader.ReadString(1, 3).has_value());
  CHECK(*reader.ReadString(1, 3) == "ell");
}

TEST_CASE("BinaryReader::ReadSpan<T>/ReadVector<T> with count == 0 succeed without reading "
          "any bytes, even past the end of the buffer",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {1, 2};
  BinaryReader reader(bytes.data(), bytes.size());
  const auto span = reader.ReadSpan<std::uint32_t>(100, 0); // offset way out of range
  REQUIRE(span.has_value());
  CHECK(span->empty());
}

TEST_CASE("BinaryReader::ReadSpan<uint32_t>/ReadVector<uint32_t> decode multiple little-endian "
          "words",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
  BinaryReader reader(bytes.data(), bytes.size());
  const auto vec = reader.ReadVector<std::uint32_t>(0, 2);
  REQUIRE(vec.has_value());
  REQUIRE(vec->size() == 2);
  CHECK((*vec)[0] == 1u);
  CHECK((*vec)[1] == 2u);

  CHECK_FALSE(reader.ReadVector<std::uint32_t>(0, 3).has_value()); // 3*4 > 8 bytes available
}

TEST_CASE("BinaryReader::ReadVector<T> copies from a misaligned offset",
          "[data][binary_reader]") {
  // Retail ADT files place MCVT/MCNR/MDDF/... payloads at offsets that are
  // not 4-byte aligned (string chunks such as MMDX have unpadded sizes), so
  // ReadVector must not depend on alignment.
  const std::vector<std::uint8_t> bytes = {0xAA, 0x01, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x80, 0x3F};
  BinaryReader reader(bytes.data(), bytes.size());
  const auto words = reader.ReadVector<std::uint32_t>(1, 1);
  REQUIRE(words.has_value());
  CHECK((*words)[0] == 1u);

  const auto floats = reader.ReadVector<float>(5, 1);
  REQUIRE(floats.has_value());
  CHECK((*floats)[0] == 1.0F);
}

TEST_CASE("BinaryReader::ReadSpan<T> rejects a misaligned offset instead of forming a "
          "misaligned pointer",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {0xAA, 0x01, 0x00, 0x00, 0x00};
  BinaryReader reader(bytes.data(), bytes.size());
  CHECK_FALSE(reader.ReadSpan<std::uint32_t>(1, 1).has_value());
}

TEST_CASE("BinaryReader array reads reject counts whose byte size overflows",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes(16, 0);
  BinaryReader reader(bytes.data(), bytes.size());
  const std::size_t huge = (SIZE_MAX / sizeof(std::uint32_t)) + 2;
  CHECK_FALSE(reader.CanReadArray<std::uint32_t>(0, huge));
  CHECK_FALSE(reader.ReadVector<std::uint32_t>(0, huge).has_value());
  CHECK_FALSE(reader.ReadSpan<std::uint32_t>(0, huge).has_value());
  CHECK(reader.CanReadArray<std::uint32_t>(0, 4));
  CHECK_FALSE(reader.CanReadArray<std::uint32_t>(1, 4));
}

TEST_CASE("BinaryReader::ReadSpan<T>/ReadVector<T> still succeed at a properly aligned offset",
          "[data][binary_reader]") {
  const std::vector<std::uint8_t> bytes = {0x00, 0x01, 0x00, 0x00, 0x00};
  BinaryReader reader(bytes.data(), bytes.size());
  const auto vec = reader.ReadVector<std::uint32_t>(0, 1); // offset 0 -- naturally aligned
  REQUIRE(vec.has_value());
  CHECK((*vec)[0] == 0x100u);
}
