#include "openwow/data/formats/dbc/dbc_file.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <limits>

using Catch::Approx;
using openwow::data::dbc::DbcError;
using openwow::data::dbc::DbcFile;

namespace {

void AppendLe32(std::vector<std::uint8_t> &out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

// Builds a well-formed WDBC buffer: header + record_count*record_size bytes
// of (already-encoded) record data + the given string block bytes.
std::vector<std::uint8_t> BuildDbc(std::uint32_t record_count, std::uint32_t field_count,
                                   std::uint32_t record_size,
                                   const std::vector<std::uint8_t> &record_bytes,
                                   const std::vector<std::uint8_t> &string_block) {
  std::vector<std::uint8_t> out;
  AppendLe32(out, 0x43424457u); // "WDBC"
  AppendLe32(out, record_count);
  AppendLe32(out, field_count);
  AppendLe32(out, record_size);
  AppendLe32(out, static_cast<std::uint32_t>(string_block.size()));
  out.insert(out.end(), record_bytes.begin(), record_bytes.end());
  out.insert(out.end(), string_block.begin(), string_block.end());
  return out;
}

} // namespace

TEST_CASE("DbcFile::LoadFromBytes rejects buffers too small to hold the signature+record_count",
          "[data][dbc]") {
  DbcFile file;
  CHECK(file.LoadFromBytes({}) == DbcError::kTooSmall);
  CHECK(file.LoadFromBytes({1, 2, 3}) == DbcError::kTooSmall); // 3 bytes < 4
}

TEST_CASE("DbcFile::LoadFromBytes rejects a bad magic signature", "[data][dbc]") {
  DbcFile file;
  std::vector<std::uint8_t> bad_magic = {'X', 'X', 'X', 'X', 0, 0, 0, 0};
  CHECK(file.LoadFromBytes(bad_magic) == DbcError::kBadMagic);
}

TEST_CASE("DbcFile::LoadFromBytes rejects a buffer with valid magic but too short to "
          "hold record_count",
          "[data][dbc]") {
  DbcFile file;
  std::vector<std::uint8_t> data;
  AppendLe32(data, 0x43424457u); // "WDBC"
  data.push_back(0);
  data.push_back(0);
  data.push_back(0); // only 3 more bytes -- 7 total, need 8
  CHECK(file.LoadFromBytes(data) == DbcError::kTooSmall);
}

TEST_CASE("DbcFile::LoadFromBytes with record_count == 0 returns kOk but leaves the file "
          "NOT loaded",
          "[data][dbc]") {
  // FLAGGED FOR FUTURE WORK: the zero-record early return happens before
  // loaded_ is ever set to true (that assignment only happens after the
  // full 20-byte header is validated), and field_count/record_size/
  // string_block_size are never even read in this path. A caller that
  // checks only `LoadFromBytes(...) == DbcError::kOk` without also
  // checking loaded() would incorrectly treat this as a usable file.
  // Verified directly against the source rather than assumed.
  DbcFile file;
  std::vector<std::uint8_t> data;
  AppendLe32(data, 0x43424457u); // "WDBC"
  AppendLe32(data, 0u);          // record_count == 0
  // Deliberately exactly 8 bytes: field_count/record_size/string_block_size
  // are never read for this path, so they don't need to be present.

  CHECK(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK_FALSE(file.loaded());
  CHECK(file.record_count() == 0);
}

TEST_CASE("DbcFile::LoadFromBytes with record_count > 0 rejects a buffer too short for "
          "the full 20-byte header",
          "[data][dbc]") {
  DbcFile file;
  std::vector<std::uint8_t> data;
  AppendLe32(data, 0x43424457u);
  AppendLe32(data, 1u); // record_count == 1
  data.push_back(0);
  data.push_back(0);
  data.push_back(0);
  data.push_back(0); // 12 bytes total, need 20 for the full header
  CHECK(file.LoadFromBytes(data) == DbcError::kTooSmall);
}

TEST_CASE("DbcFile::LoadFromBytes rejects a header whose declared sizes don't fit the buffer",
          "[data][dbc]") {
  DbcFile file;
  // A full 20-byte header claiming 1 record of 4 bytes each, but the
  // buffer stops right after the header -- no room for that record.
  std::vector<std::uint8_t> data;
  AppendLe32(data, 0x43424457u);
  AppendLe32(data, 1u); // record_count
  AppendLe32(data, 1u); // field_count
  AppendLe32(data, 4u); // record_size
  AppendLe32(data, 0u); // string_block_size
  CHECK(data.size() == 20);
  CHECK(file.LoadFromBytes(data) == DbcError::kInconsistentSize);
}

TEST_CASE("DbcFile::LoadFromBytes accepts a well-formed single-record, single-field file",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes;
  AppendLe32(record_bytes, 42u);
  auto data = BuildDbc(1, 1, 4, record_bytes, {});

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK(file.loaded());
  CHECK(file.record_count() == 1);
  CHECK(file.field_count() == 1);
  CHECK(file.record_size() == 4);
  CHECK(file.GetUInt32(0, 0) == 42u);
}

TEST_CASE("DbcFile::GetInt32/GetFloat reinterpret the same bit pattern as GetUInt32",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes;
  AppendLe32(record_bytes, 0xFFFFFFFFu); // -1 as int32
  AppendLe32(record_bytes, 0x3FC00000u); // 1.5f as IEEE-754 bits
  auto data = BuildDbc(1, 2, 8, record_bytes, {});

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK(file.GetInt32(0, 0) == -1);
  CHECK(file.GetFloat(0, 1) == Approx(1.5f));
}

TEST_CASE("DbcFile accessors return zero for an out-of-range record or field index",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes;
  AppendLe32(record_bytes, 42u);
  auto data = BuildDbc(1, 1, 4, record_bytes, {});

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK(file.GetUInt32(1, 0) == 0u); // record index out of range (only record 0 exists)
  CHECK(file.GetUInt32(0, 1) == 0u); // field index out of range (only field 0 exists)
}

TEST_CASE("DbcFile::GetString reads a null-terminated string from the string block by offset",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes;
  AppendLe32(record_bytes, 0u); // record 0's string field points at offset 0
  AppendLe32(record_bytes, 6u); // record 0's second field points at offset 6
  std::vector<std::uint8_t> string_block(reinterpret_cast<const std::uint8_t *>("Hello\0World\0"),
                                         reinterpret_cast<const std::uint8_t *>("Hello\0World\0") +
                                             12);
  auto data = BuildDbc(1, 2, 8, record_bytes, string_block);

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK(file.GetString(0, 0) == "Hello");
  CHECK(file.GetString(0, 1) == "World");
}

TEST_CASE("DbcFile::GetString returns empty for an offset at or past the string block's size",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes;
  AppendLe32(record_bytes, 12u); // == string_block_size, out of range
  std::vector<std::uint8_t> string_block(reinterpret_cast<const std::uint8_t *>("Hello\0World\0"),
                                         reinterpret_cast<const std::uint8_t *>("Hello\0World\0") +
                                             12);
  auto data = BuildDbc(1, 1, 4, record_bytes, string_block);

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK(file.GetString(0, 0) == "");
}

TEST_CASE("DbcFile::GetString returns empty rather than reading past an unterminated "
          "string block",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes;
  AppendLe32(record_bytes, 0u);
  std::vector<std::uint8_t> string_block = {'A', 'B'}; // no null terminator anywhere
  auto data = BuildDbc(1, 1, 4, record_bytes, string_block);

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK(file.GetString(0, 0) == "");
}

TEST_CASE("DbcFile::GetLocalizedString at the default (en-US, index 0) locale matches "
          "GetString at the same field",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes;
  AppendLe32(record_bytes, 0u);
  std::vector<std::uint8_t> string_block(reinterpret_cast<const std::uint8_t *>("Hi\0"),
                                         reinterpret_cast<const std::uint8_t *>("Hi\0") + 3);
  auto data = BuildDbc(1, 1, 4, record_bytes, string_block);

  DbcFile file; // default-constructed -> DbcLocale::kEnUs, index 0
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK(file.GetLocalizedString(0, 0) == file.GetString(0, 0));
  CHECK(file.GetLocalizedString(0, 0) == "Hi");
}

TEST_CASE("DbcFile::GetLocalizedString returns empty when first_field + locale index "
          "would overflow or exceed field_count",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes;
  AppendLe32(record_bytes, 0u);
  auto data = BuildDbc(1, 1, 4, record_bytes, {});

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  // first_field near uint32 max makes `first_field + localeIndex` wrap
  // around past 0, so the `localized_field >= first_field` guard should
  // reject it rather than wrapping into a small, seemingly-valid index.
  CHECK(file.GetLocalizedString(0, std::numeric_limits<std::uint32_t>::max()) == "");
}

TEST_CASE("DbcFile byte-offset accessors (GetByte/GetUInt32AtOffset/GetFloatAtOffset) "
          "address raw bytes within a record directly",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes = {0xAB, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x3F};
  // byte 0: 0xAB; bytes 4..7: 1.5f (0x3FC00000 little-endian).
  auto data = BuildDbc(1, 2, 8, record_bytes, {});

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  CHECK(file.GetByte(0, 0) == 0xAB);
  CHECK(file.GetFloatAtOffset(0, 4) == Approx(1.5f));
}

TEST_CASE("DbcFile byte-offset accessors return zero when the requested span would run "
          "past the end of the record",
          "[data][dbc]") {
  std::vector<std::uint8_t> record_bytes = {1, 2, 3, 4}; // record_size == 4
  auto data = BuildDbc(1, 1, 4, record_bytes, {});

  DbcFile file;
  REQUIRE(file.LoadFromBytes(data) == DbcError::kOk);
  // byte_offset(1) + 4 bytes == 5, past the 4-byte record.
  CHECK(file.GetUInt32AtOffset(0, 1) == 0u);
}

TEST_CASE("DbcFile::GetErrorName maps every DbcError to a distinct, exact string", "[data][dbc]") {
  CHECK(std::string_view(DbcFile::GetErrorName(DbcError::kOk)) == "Ok");
  CHECK(std::string_view(DbcFile::GetErrorName(DbcError::kTooSmall)) == "TooSmall");
  CHECK(std::string_view(DbcFile::GetErrorName(DbcError::kBadMagic)) == "BadMagic");
  CHECK(std::string_view(DbcFile::GetErrorName(DbcError::kInconsistentSize)) == "InconsistentSize");
}
