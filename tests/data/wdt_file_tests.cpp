#include "openwow/data/terrain/wdt_file.h"

#include "openwow/data/wow_chunk_fourcc.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using openwow::data::WowChunkFourCC;
using openwow::data::terrain::LoadWdt;
using openwow::data::terrain::WdtWmoPlacement;
namespace WdtFlags = openwow::data::terrain::WdtFlags;

namespace {

void AppendLe32(std::vector<std::uint8_t> &out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

void AppendChunk(std::vector<std::uint8_t> &out, const char (&tag)[5],
                 const std::vector<std::uint8_t> &payload) {
  AppendLe32(out, WowChunkFourCC(tag));
  AppendLe32(out, static_cast<std::uint32_t>(payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
}

std::vector<std::uint8_t> MakeMverPayload(std::uint32_t version = 18) {
  std::vector<std::uint8_t> payload;
  AppendLe32(payload, version);
  return payload;
}

std::vector<std::uint8_t> MakeMphdPayload(std::uint32_t flags = 0) {
  std::vector<std::uint8_t> payload;
  AppendLe32(payload, flags);
  return payload;
}

// 64x64 grid of {flags, async_id}, all zero by default (no tiles exist).
std::vector<std::uint8_t> MakeMainPayload(std::uint32_t existing_x = 0xFFFFFFFF,
                                          std::uint32_t existing_y = 0xFFFFFFFF) {
  std::vector<std::uint8_t> payload;
  for (std::uint32_t y = 0; y < 64; ++y) {
    for (std::uint32_t x = 0; x < 64; ++x) {
      const std::uint32_t flags = (x == existing_x && y == existing_y) ? 1u : 0u;
      AppendLe32(payload, flags);
      AppendLe32(payload, 0u); // async_id
    }
  }
  return payload;
}

// Assembles a minimal, otherwise-valid WDT (MVER + MPHD + MAIN) and lets
// the caller override/omit/append chunks around that baseline.
std::vector<std::uint8_t> BuildMinimalWdt(bool include_mver = true, bool include_mphd = true,
                                          bool include_main = true, std::uint32_t mver_version = 18,
                                          std::uint32_t mphd_flags = 0) {
  std::vector<std::uint8_t> data;
  if (include_mver)
    AppendChunk(data, "MVER", MakeMverPayload(mver_version));
  if (include_mphd)
    AppendChunk(data, "MPHD", MakeMphdPayload(mphd_flags));
  if (include_main)
    AppendChunk(data, "MAIN", MakeMainPayload());
  return data;
}

} // namespace

TEST_CASE("LoadWdt rejects null or too-small input", "[data][wdt]") {
  CHECK_FALSE(LoadWdt(nullptr, 0).ok);
  const std::vector<std::uint8_t> tiny = {1, 2, 3};
  CHECK_FALSE(LoadWdt(tiny).ok);
}

TEST_CASE("LoadWdt requires MVER, MPHD, and MAIN chunks to all be present", "[data][wdt]") {
  CHECK_FALSE(LoadWdt(BuildMinimalWdt(/*include_mver=*/false)).ok);
  CHECK_FALSE(LoadWdt(BuildMinimalWdt(/*include_mver=*/true, /*include_mphd=*/false)).ok);
  CHECK_FALSE(LoadWdt(BuildMinimalWdt(true, true, /*include_main=*/false)).ok);
}

TEST_CASE("LoadWdt rejects an MVER version other than 18", "[data][wdt]") {
  const auto result = LoadWdt(BuildMinimalWdt(true, true, true, /*mver_version=*/17));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("18") != std::string::npos);
}

TEST_CASE("LoadWdt rejects an MPHD chunk smaller than 4 bytes", "[data][wdt]") {
  std::vector<std::uint8_t> data;
  AppendChunk(data, "MVER", MakeMverPayload());
  AppendChunk(data, "MPHD", {1, 2, 3}); // 3 bytes, need at least 4
  AppendChunk(data, "MAIN", MakeMainPayload());
  CHECK_FALSE(LoadWdt(data).ok);
}

TEST_CASE("LoadWdt rejects an undersized MAIN chunk", "[data][wdt]") {
  std::vector<std::uint8_t> data;
  AppendChunk(data, "MVER", MakeMverPayload());
  AppendChunk(data, "MPHD", MakeMphdPayload());
  AppendChunk(data, "MAIN", {1, 2, 3, 4}); // way short of 64*64*8 bytes
  const auto result = LoadWdt(data);
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("32768") != std::string::npos);
}

TEST_CASE("LoadWdt rejects a chunk whose declared size extends past the end of the file",
          "[data][wdt]") {
  std::vector<std::uint8_t> data;
  AppendLe32(data, WowChunkFourCC("MVER"));
  AppendLe32(data, 100u); // claims 100 bytes of payload
  AppendLe32(data, 18u);  // but only 4 bytes actually follow
  const auto result = LoadWdt(data);
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("extends past end") != std::string::npos);
}

TEST_CASE("LoadWdt parses a minimal valid WDT and reports version/flags/no tiles", "[data][wdt]") {
  const auto data = BuildMinimalWdt(true, true, true, 18, WdtFlags::kMccvInAdts);
  const auto result = LoadWdt(data);
  REQUIRE(result.ok);
  CHECK(result.wdt.version == 18);
  CHECK(result.wdt.flags == WdtFlags::kMccvInAdts);
  CHECK_FALSE(result.wdt.has_global_wmo); // kGlobalWmo bit not set
  CHECK(result.wdt.CountExistingTiles() == 0);
  CHECK_FALSE(result.wdt.TileExists(0, 0));
}

TEST_CASE("LoadWdt correctly reads which MAIN tiles exist and rejects out-of-range coordinates",
          "[data][wdt]") {
  std::vector<std::uint8_t> data;
  AppendChunk(data, "MVER", MakeMverPayload());
  AppendChunk(data, "MPHD", MakeMphdPayload());
  AppendChunk(data, "MAIN", MakeMainPayload(/*existing_x=*/5, /*existing_y=*/7));
  const auto result = LoadWdt(data);
  REQUIRE(result.ok);
  CHECK(result.wdt.TileExists(5, 7));
  CHECK_FALSE(result.wdt.TileExists(6, 7));
  CHECK(result.wdt.CountExistingTiles() == 1);
  CHECK_FALSE(result.wdt.TileExists(64, 0)); // x out of the 0..63 range
  CHECK_FALSE(result.wdt.TileExists(0, 64)); // y out of the 0..63 range
}

TEST_CASE("LoadWdt reads the global WMO path from MWMO as the first null-terminated string",
          "[data][wdt]") {
  std::vector<std::uint8_t> data;
  AppendChunk(data, "MVER", MakeMverPayload());
  AppendChunk(data, "MPHD", MakeMphdPayload(WdtFlags::kGlobalWmo));
  AppendChunk(data, "MAIN", MakeMainPayload());
  std::vector<std::uint8_t> mwmo_payload(
      reinterpret_cast<const std::uint8_t *>("World\\wmo\\Test.wmo\0Ignored.wmo\0"),
      reinterpret_cast<const std::uint8_t *>("World\\wmo\\Test.wmo\0Ignored.wmo\0") + 32);
  AppendChunk(data, "MWMO", mwmo_payload);

  const auto result = LoadWdt(data);
  REQUIRE(result.ok);
  CHECK(result.wdt.has_global_wmo);
  CHECK(result.wdt.global_wmo_path == "World\\wmo\\Test.wmo");
}

TEST_CASE("LoadWdt tolerates an empty MWMO chunk (leaves global_wmo_path empty)", "[data][wdt]") {
  std::vector<std::uint8_t> data;
  AppendChunk(data, "MVER", MakeMverPayload());
  AppendChunk(data, "MPHD", MakeMphdPayload());
  AppendChunk(data, "MAIN", MakeMainPayload());
  AppendChunk(data, "MWMO", {}); // zero-length payload

  const auto result = LoadWdt(data);
  REQUIRE(result.ok);
  CHECK(result.wdt.global_wmo_path.empty());
}

TEST_CASE("LoadWdt copies a full-size MODF chunk directly into global_wmo_placement",
          "[data][wdt]") {
  WdtWmoPlacement placement{};
  placement.name_id = 7;
  placement.unique_id = 99;
  placement.position[0] = 1.0f;
  placement.position[1] = 2.0f;
  placement.position[2] = 3.0f;
  placement.flags = 0x1234;
  placement.scale = 1024;

  std::vector<std::uint8_t> modf_payload(sizeof(WdtWmoPlacement));
  std::memcpy(modf_payload.data(), &placement, sizeof(WdtWmoPlacement));

  std::vector<std::uint8_t> data;
  AppendChunk(data, "MVER", MakeMverPayload());
  AppendChunk(data, "MPHD", MakeMphdPayload());
  AppendChunk(data, "MAIN", MakeMainPayload());
  AppendChunk(data, "MODF", modf_payload);

  const auto result = LoadWdt(data);
  REQUIRE(result.ok);
  CHECK(result.wdt.global_wmo_placement.name_id == 7);
  CHECK(result.wdt.global_wmo_placement.unique_id == 99);
  CHECK(result.wdt.global_wmo_placement.position[0] == 1.0f);
  CHECK(result.wdt.global_wmo_placement.position[2] == 3.0f);
  CHECK(result.wdt.global_wmo_placement.flags == 0x1234);
  CHECK(result.wdt.global_wmo_placement.scale == 1024);
}

TEST_CASE("LoadWdt ignores an undersized MODF chunk rather than reading past it", "[data][wdt]") {
  std::vector<std::uint8_t> data;
  AppendChunk(data, "MVER", MakeMverPayload());
  AppendChunk(data, "MPHD", MakeMphdPayload());
  AppendChunk(data, "MAIN", MakeMainPayload());
  AppendChunk(data, "MODF", {1, 2, 3, 4}); // far short of sizeof(WdtWmoPlacement)

  const auto result = LoadWdt(data);
  REQUIRE(result.ok); // MODF is optional; an undersized one is simply skipped
  CHECK(result.wdt.global_wmo_placement.name_id == 0);
}

TEST_CASE("LoadWdt silently skips chunks it doesn't recognize", "[data][wdt]") {
  std::vector<std::uint8_t> data;
  AppendChunk(data, "MVER", MakeMverPayload());
  AppendChunk(data, "FOOB", {9, 9, 9, 9}); // unrecognized chunk between required ones
  AppendChunk(data, "MPHD", MakeMphdPayload());
  AppendChunk(data, "MAIN", MakeMainPayload());

  CHECK(LoadWdt(data).ok);
}
