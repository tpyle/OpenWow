#include "openwow/network/serialization/byte_buffer.h"
#include "openwow/network/serialization/packed_guid_codec.h"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using openwow::net::ByteBuffer;
using openwow::net::DecodePackedGuid;
using openwow::net::EncodePackedGuid;
using openwow::net::PackedGuidMask;

TEST_CASE("ByteBuffer starts empty", "[byte_buffer]") {
  ByteBuffer buffer;
  CHECK(buffer.IsEmpty());
  CHECK(buffer.GetSize() == 0);
  CHECK(buffer.GetReadOffset() == 0);
  CHECK(buffer.GetRemainingBytes() == 0);
  CHECK(buffer.IsFullyConsumed());
  CHECK(buffer.CanRead(0));
  CHECK_FALSE(buffer.CanRead(1));
}

TEST_CASE("ByteBuffer round-trips fixed-width integers", "[byte_buffer]") {
  ByteBuffer buffer;

  SECTION("uint8_t") {
    for (const uint8_t value : {uint8_t{0}, uint8_t{1}, uint8_t{0x7F}, uint8_t{0xFF}}) {
      buffer.Clear();
      buffer.WriteUInt8(value);
      uint8_t out = 0;
      REQUIRE(buffer.ReadUInt8(&out));
      CHECK(out == value);
      CHECK(buffer.IsFullyConsumed());
    }
  }

  SECTION("uint16_t") {
    for (const uint16_t value : {uint16_t{0}, uint16_t{1}, uint16_t{0x1234}, uint16_t{0xFFFF}}) {
      buffer.Clear();
      buffer.WriteUInt16(value);
      uint16_t out = 0;
      REQUIRE(buffer.ReadUInt16(&out));
      CHECK(out == value);
    }
  }

  SECTION("uint32_t") {
    for (const uint32_t value :
         {uint32_t{0}, uint32_t{1}, uint32_t{0x12345678}, uint32_t{0xFFFFFFFF}}) {
      buffer.Clear();
      buffer.WriteUInt32(value);
      uint32_t out = 0;
      REQUIRE(buffer.ReadUInt32(&out));
      CHECK(out == value);
    }
  }

  SECTION("uint64_t") {
    for (const uint64_t value : {uint64_t{0}, uint64_t{1}, uint64_t{0x0123456789ABCDEFULL},
                                 std::numeric_limits<uint64_t>::max()}) {
      buffer.Clear();
      buffer.WriteUInt64(value);
      uint64_t out = 0;
      REQUIRE(buffer.ReadUInt64(&out));
      CHECK(out == value);
    }
  }

  SECTION("signed integers preserve sign") {
    buffer.Clear();
    buffer.WriteInt8(-1);
    buffer.WriteInt16(-1000);
    buffer.WriteInt32(-100000);
    int8_t i8 = 0;
    int16_t i16 = 0;
    int32_t i32 = 0;
    REQUIRE(buffer.ReadInt8(&i8));
    REQUIRE(buffer.ReadInt16(&i16));
    REQUIRE(buffer.ReadInt32(&i32));
    CHECK(i8 == -1);
    CHECK(i16 == -1000);
    CHECK(i32 == -100000);
  }

  SECTION("float round-trips exactly, including special values") {
    for (const float value : {0.0f, -0.0f, 1.0f, -123.456f, std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()}) {
      buffer.Clear();
      buffer.WriteFloat(value);
      float out = 0.0f;
      REQUIRE(buffer.ReadFloat(&out));
      CHECK(out == value);
    }
  }
}

TEST_CASE("ByteBuffer encodes multi-byte integers little-endian", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.WriteUInt32(0x12345678u);
  REQUIRE(buffer.GetSize() == 4);
  CHECK(buffer.Data()[0] == 0x78);
  CHECK(buffer.Data()[1] == 0x56);
  CHECK(buffer.Data()[2] == 0x34);
  CHECK(buffer.Data()[3] == 0x12);
}

TEST_CASE("ByteBuffer reads sequentially across mixed types", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.WriteUInt8(0xAB);
  buffer.WriteUInt32(42);
  buffer.WriteFloat(3.5f);
  buffer.WriteString("hello");

  uint8_t u8 = 0;
  uint32_t u32 = 0;
  float f = 0.0f;
  std::string s;
  REQUIRE(buffer.ReadUInt8(&u8));
  REQUIRE(buffer.ReadUInt32(&u32));
  REQUIRE(buffer.ReadFloat(&f));
  REQUIRE(buffer.ReadString(s));
  CHECK(u8 == 0xAB);
  CHECK(u32 == 42);
  CHECK(f == 3.5f);
  CHECK(s == "hello");
  CHECK(buffer.IsFullyConsumed());
}

TEST_CASE("ByteBuffer string handling", "[byte_buffer]") {
  SECTION("empty string round-trips") {
    ByteBuffer buffer;
    buffer.WriteString("");
    REQUIRE(buffer.GetSize() == 1); // just the null terminator
    std::string out = "not empty";
    REQUIRE(buffer.ReadString(out));
    CHECK(out.empty());
  }

  SECTION("string is null-terminated on the wire") {
    ByteBuffer buffer;
    buffer.WriteString("hi");
    REQUIRE(buffer.GetSize() == 3);
    CHECK(buffer.Data()[0] == 'h');
    CHECK(buffer.Data()[1] == 'i');
    CHECK(buffer.Data()[2] == 0);
  }

  SECTION("reading a string with no terminator consumes to the end and still succeeds") {
    ByteBuffer buffer;
    buffer.WriteUInt8('n');
    buffer.WriteUInt8('o');
    buffer.WriteUInt8('t');
    std::string out;
    REQUIRE(buffer.ReadString(out));
    CHECK(out == "not");
    CHECK(buffer.IsFullyConsumed());
  }
}

TEST_CASE("ByteBuffer WriteBytes/ReadBytes round-trip raw data", "[byte_buffer]") {
  ByteBuffer buffer;
  const uint8_t payload[] = {1, 2, 3, 4, 5};
  buffer.WriteBytes(payload, sizeof(payload));
  REQUIRE(buffer.GetSize() == sizeof(payload));

  uint8_t out[5] = {};
  REQUIRE(buffer.ReadBytes(out, sizeof(out)));
  CHECK(std::equal(std::begin(payload), std::end(payload), std::begin(out)));
}

TEST_CASE("ByteBuffer WriteBytes tolerates null/zero-length input", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.WriteBytes(nullptr, 0);
  buffer.WriteBytes(nullptr, 5);
  CHECK(buffer.IsEmpty());
}

TEST_CASE("ByteBuffer read failures do not advance the read offset", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.WriteUInt8(1);

  SECTION("reading more than available fails and leaves offset untouched") {
    uint32_t out = 0;
    CHECK_FALSE(buffer.ReadUInt32(&out));
    CHECK(buffer.GetReadOffset() == 0);
  }

  SECTION("reading with a null output pointer fails") {
    CHECK_FALSE(buffer.ReadUInt8(nullptr));
    CHECK(buffer.GetReadOffset() == 0);
  }

  SECTION("reading bytes past the end fails") {
    uint8_t out[10] = {};
    CHECK_FALSE(buffer.ReadBytes(out, 10));
    CHECK(buffer.GetReadOffset() == 0);
  }

  SECTION("a fully-consumed buffer cannot be read further") {
    uint8_t out = 0;
    REQUIRE(buffer.ReadUInt8(&out));
    CHECK_FALSE(buffer.ReadUInt8(&out));
  }
}

TEST_CASE("ByteBuffer CanRead/GetRemainingBytes stay consistent while reading", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.WriteUInt32(1);
  buffer.WriteUInt32(2);

  CHECK(buffer.GetRemainingBytes() == 8);
  CHECK(buffer.CanRead(8));
  CHECK_FALSE(buffer.CanRead(9));

  uint32_t out = 0;
  REQUIRE(buffer.ReadUInt32(&out));
  CHECK(buffer.GetRemainingBytes() == 4);
  CHECK(buffer.CanRead(4));
  CHECK_FALSE(buffer.CanRead(5));

  REQUIRE(buffer.ReadUInt32(&out));
  CHECK(buffer.GetRemainingBytes() == 0);
  CHECK(buffer.IsFullyConsumed());
}

TEST_CASE("ByteBuffer ResetReadOffset allows re-reading", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.WriteUInt32(7);
  uint32_t out = 0;
  REQUIRE(buffer.ReadUInt32(&out));
  CHECK(buffer.IsFullyConsumed());

  buffer.ResetReadOffset();
  CHECK(buffer.GetReadOffset() == 0);
  REQUIRE(buffer.ReadUInt32(&out));
  CHECK(out == 7);
}

TEST_CASE("ByteBuffer Clear resets both data and read offset", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.WriteUInt32(1);
  uint32_t out = 0;
  REQUIRE(buffer.ReadUInt32(&out));

  buffer.Clear();
  CHECK(buffer.IsEmpty());
  CHECK(buffer.GetReadOffset() == 0);
  CHECK(buffer.GetSize() == 0);
}

TEST_CASE("ByteBuffer Append concatenates raw data, not read state", "[byte_buffer]") {
  ByteBuffer a;
  a.WriteUInt8(1);
  a.WriteUInt8(2);

  ByteBuffer b;
  b.WriteUInt8(3);
  b.WriteUInt8(4);

  a.Append(b);
  REQUIRE(a.GetSize() == 4);
  CHECK(a.Data()[0] == 1);
  CHECK(a.Data()[1] == 2);
  CHECK(a.Data()[2] == 3);
  CHECK(a.Data()[3] == 4);
  // Appending must not touch b's own contents.
  CHECK(b.GetSize() == 2);
}

TEST_CASE("ByteBuffer Detach hands over ownership and resets the buffer", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.WriteUInt8(9);
  buffer.WriteUInt8(8);

  const auto detached = buffer.Detach();
  REQUIRE(detached.size() == 2);
  CHECK(detached[0] == 9);
  CHECK(detached[1] == 8);
  CHECK(buffer.IsEmpty());
  CHECK(buffer.GetReadOffset() == 0);
}

TEST_CASE("ByteBuffer Reserve does not change observable size", "[byte_buffer]") {
  ByteBuffer buffer;
  buffer.Reserve(1024);
  CHECK(buffer.IsEmpty());
  CHECK(buffer.GetSize() == 0);
}

// --- Packed GUID codec -------------------------------------------------
//
// WoW's wire format for a GUID sends a leading bitmask of which of the 8
// bytes are non-zero, followed by only those non-zero bytes -- so a GUID
// with lots of zero bytes (the common case for low-numbered entities)
// takes far fewer than 8 bytes on the wire.

TEST_CASE("PackedGuidMask flags exactly the non-zero bytes", "[byte_buffer][packed_guid]") {
  CHECK(PackedGuidMask(0) == 0x00);
  CHECK(PackedGuidMask(0x00000000000000FFULL) == 0x01);
  CHECK(PackedGuidMask(0x000000000000FF00ULL) == 0x02);
  CHECK(PackedGuidMask(0xFF00000000000000ULL) == 0x80);
  CHECK(PackedGuidMask(0xFFFFFFFFFFFFFFFFULL) == 0xFF);
  // Byte 0 = 0x01, byte 2 = 0x01 -> bits 0 and 2 set.
  CHECK(PackedGuidMask(0x0000000000010001ULL) == 0b00000101);
}

TEST_CASE("EncodePackedGuid emits mask byte plus only the non-zero bytes",
          "[byte_buffer][packed_guid]") {
  SECTION("zero guid encodes as just a zero mask byte") {
    const auto encoded = EncodePackedGuid(0);
    CHECK(encoded.size == 1);
    CHECK(encoded.bytes[0] == 0x00);
  }

  SECTION("single low byte") {
    const auto encoded = EncodePackedGuid(0x42);
    REQUIRE(encoded.size == 2);
    CHECK(encoded.bytes[0] == 0x01);
    CHECK(encoded.bytes[1] == 0x42);
  }

  SECTION("all bytes non-zero encodes to the full 9 bytes") {
    const auto encoded = EncodePackedGuid(0x0102030405060708ULL);
    REQUIRE(encoded.size == 9);
    CHECK(encoded.bytes[0] == 0xFF);
    CHECK(encoded.bytes[1] == 0x08);
    CHECK(encoded.bytes[8] == 0x01);
  }

  SECTION("sparse bytes are packed in ascending byte-index order") {
    // byte0 = 0xAA, byte3 = 0xBB
    const auto encoded = EncodePackedGuid(0x00000000BB0000AAULL);
    REQUIRE(encoded.size == 3);
    CHECK(encoded.bytes[0] == 0b00001001); // bits 0 and 3
    CHECK(encoded.bytes[1] == 0xAA);
    CHECK(encoded.bytes[2] == 0xBB);
  }
}

TEST_CASE("DecodePackedGuid is the exact inverse of EncodePackedGuid",
          "[byte_buffer][packed_guid]") {
  for (const uint64_t guid :
       {uint64_t{0}, uint64_t{1}, uint64_t{0x42}, uint64_t{0x0102030405060708ULL},
        uint64_t{0x00000000BB0000AAULL}, std::numeric_limits<uint64_t>::max()}) {
    const auto encoded = EncodePackedGuid(guid);
    const auto decoded = DecodePackedGuid(encoded.view());
    REQUIRE(static_cast<bool>(decoded));
    CHECK(decoded.value == guid);
    CHECK(decoded.bytes_consumed == encoded.size);
  }
}

TEST_CASE("DecodePackedGuid rejects truncated input", "[byte_buffer][packed_guid]") {
  SECTION("empty input") {
    const auto decoded = DecodePackedGuid(std::span<const uint8_t>{});
    CHECK_FALSE(static_cast<bool>(decoded));
  }

  SECTION("mask claims a byte that is not present") {
    const std::array<uint8_t, 1> truncated = {0x01}; // mask says 1 byte follows
    const auto decoded = DecodePackedGuid(std::span<const uint8_t>(truncated));
    CHECK_FALSE(static_cast<bool>(decoded));
  }
}

TEST_CASE("ByteBuffer WritePackedGuid/ReadPackedGuid round-trip through the wire format",
          "[byte_buffer][packed_guid]") {
  for (const uint64_t guid : {uint64_t{0}, uint64_t{0x42}, uint64_t{0x0102030405060708ULL},
                              std::numeric_limits<uint64_t>::max()}) {
    ByteBuffer buffer;
    buffer.WritePackedGuid(guid);
    uint64_t out = 0;
    REQUIRE(buffer.ReadPackedGuid(&out));
    CHECK(out == guid);
    CHECK(buffer.IsFullyConsumed());
  }
}

TEST_CASE("ByteBuffer ReadPackedGuid fails on an empty buffer", "[byte_buffer][packed_guid]") {
  ByteBuffer buffer;
  uint64_t out = 0;
  CHECK_FALSE(buffer.ReadPackedGuid(&out));
}
