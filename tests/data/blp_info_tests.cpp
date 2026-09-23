#include "openwow/data/blp/blp_info.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <vector>

using openwow::data::BLPAlphaType;
using openwow::data::BLPCompression;
using openwow::data::BLPInfoReader;
using openwow::data::BLPTextureInfo;

namespace {

constexpr std::size_t kHeaderSize = 4 + 4 + 1 + 1 + 1 + 1 + 4 + 4 + 16 * 4 + 16 * 4; // 148

void PutLe32(std::vector<std::uint8_t> &out, std::size_t offset, std::uint32_t value) {
  out[offset + 0] = static_cast<std::uint8_t>(value & 0xFF);
  out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
  out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
  out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

// Builds a minimal, well-formed BLP2 header. mip_sizes defaults to a
// single 100-byte mip at offset 0; pass an explicit vector to control the
// mip table (including gaps, for the "stops at first zero" test).
std::vector<std::uint8_t> BuildBlpHeader(std::uint32_t width, std::uint32_t height,
                                         std::uint8_t compression = 3, std::uint8_t alpha_depth = 0,
                                         std::uint8_t has_mips = 0,
                                         std::vector<std::uint32_t> mip_sizes = {100},
                                         std::uint32_t type = 1) {
  std::vector<std::uint8_t> data(kHeaderSize, 0);
  PutLe32(data, 0, 0x32504C42u); // "BLP2"
  PutLe32(data, 4, type);
  data[8] = compression;
  data[9] = alpha_depth;
  data[10] = alpha_depth; // alphaType byte is unused by ParseHeader; keep it harmless
  data[11] = has_mips;
  PutLe32(data, 12, width);
  PutLe32(data, 16, height);
  // mipOffsets[16] at byte 20, mipSizes[16] at byte 20 + 64 = 84.
  for (std::size_t i = 0; i < mip_sizes.size() && i < 16; ++i) {
    PutLe32(data, 84 + i * 4, mip_sizes[i]);
  }
  return data;
}

} // namespace

TEST_CASE("BLPInfoReader::ParseHeader rejects null data or a buffer smaller than the header",
          "[data][blp]") {
  CHECK_FALSE(BLPInfoReader::ParseHeader(nullptr, 0).has_value());
  const std::vector<std::uint8_t> too_small(kHeaderSize - 1, 0);
  CHECK_FALSE(BLPInfoReader::ParseHeader(too_small.data(), too_small.size()).has_value());
}

TEST_CASE("BLPInfoReader::ParseHeader rejects a non-BLP2 magic", "[data][blp]") {
  auto data = BuildBlpHeader(64, 64);
  PutLe32(data, 0, 0x11223344u);
  CHECK_FALSE(BLPInfoReader::ParseHeader(data.data(), data.size()).has_value());
}

TEST_CASE("BLPInfoReader::ParseHeader rejects zero width or height", "[data][blp]") {
  auto zero_width = BuildBlpHeader(0, 64);
  CHECK_FALSE(BLPInfoReader::ParseHeader(zero_width.data(), zero_width.size()).has_value());
  auto zero_height = BuildBlpHeader(64, 0);
  CHECK_FALSE(BLPInfoReader::ParseHeader(zero_height.data(), zero_height.size()).has_value());
}

TEST_CASE("BLPInfoReader::ParseHeader decodes compression/alpha/dimensions/hasMips",
          "[data][blp]") {
  auto data = BuildBlpHeader(64, 32, /*compression=*/2, /*alpha_depth=*/8, /*has_mips=*/1);
  const auto info = BLPInfoReader::ParseHeader(data.data(), data.size());
  REQUIRE(info.has_value());
  CHECK(info->compression == BLPCompression::DirectX);
  CHECK(info->alphaType == BLPAlphaType::Alpha8Bit);
  CHECK(info->hasMips);
  CHECK(info->width == 64);
  CHECK(info->height == 32);
}

TEST_CASE("BLPInfoReader::ParseHeader falls back to JPEG/NoAlpha for out-of-range enum bytes",
          "[data][blp]") {
  auto data = BuildBlpHeader(4, 4, /*compression=*/99, /*alpha_depth=*/2);
  const auto info = BLPInfoReader::ParseHeader(data.data(), data.size());
  REQUIRE(info.has_value());
  CHECK(info->compression == BLPCompression::JPEG);
  CHECK(info->alphaType == BLPAlphaType::NoAlpha);
}

TEST_CASE("BLPInfoReader::ParseHeader's mip count stops at the first zero-sized entry, "
          "even if a later entry is nonzero",
          "[data][blp]") {
  // FLAGGED FOR FUTURE WORK (documented, not fixed): a gap in mipSizes[]
  // silently truncates the reported mip chain rather than being treated
  // as malformed. Verified directly rather than assumed.
  auto data = BuildBlpHeader(64, 64, 3, 0, 1, {100, 50, 0, 200});
  const auto info = BLPInfoReader::ParseHeader(data.data(), data.size());
  REQUIRE(info.has_value());
  CHECK(info->mipCount == 2);
  REQUIRE(info->mips.size() == 2);
  CHECK(info->mips[0].width == 64);
  CHECK(info->mips[0].height == 64);
  CHECK(info->mips[1].width == 32);
  CHECK(info->mips[1].height == 32);
}

TEST_CASE("BLPInfoReader::ParseHeader halves mip dimensions, floored at 1", "[data][blp]") {
  auto data = BuildBlpHeader(8, 4, 3, 0, 1, {10, 10, 10});
  const auto info = BLPInfoReader::ParseHeader(data.data(), data.size());
  REQUIRE(info.has_value());
  REQUIRE(info->mips.size() == 3);
  CHECK(info->mips[0].width == 8);
  CHECK(info->mips[0].height == 4);
  CHECK(info->mips[1].width == 4);
  CHECK(info->mips[1].height == 2);
  CHECK(info->mips[2].width == 2);
  CHECK(info->mips[2].height == 1);
}

TEST_CASE("BLPInfoReader::ParseHeader with no mips reports mipCount 0 and an empty mip list",
          "[data][blp]") {
  auto data = BuildBlpHeader(16, 16, 3, 0, 0, {0}); // first mip size already 0
  const auto info = BLPInfoReader::ParseHeader(data.data(), data.size());
  REQUIRE(info.has_value());
  CHECK(info->mipCount == 0);
  CHECK(info->mips.empty());
}

TEST_CASE("BLPInfoReader::IsValidBLP checks both the magic and the minimum header size",
          "[data][blp]") {
  CHECK_FALSE(BLPInfoReader::IsValidBLP(nullptr, 0));
  const std::vector<std::uint8_t> just_magic = {0x42, 0x4C, 0x50, 0x32}; // "BLP2", 4 bytes only
  CHECK_FALSE(BLPInfoReader::IsValidBLP(just_magic.data(), just_magic.size()));

  auto full = BuildBlpHeader(4, 4);
  CHECK(BLPInfoReader::IsValidBLP(full.data(), full.size()));
  PutLe32(full, 0, 0);
  CHECK_FALSE(BLPInfoReader::IsValidBLP(full.data(), full.size()));
}

TEST_CASE("BLPInfoReader::GetMipCount computes the full mip chain length for a power-of-two "
          "dimension",
          "[data][blp]") {
  CHECK(BLPInfoReader::GetMipCount(0, 64) == 0);
  CHECK(BLPInfoReader::GetMipCount(1, 1) == 1);
  CHECK(BLPInfoReader::GetMipCount(2, 2) == 2);
  CHECK(BLPInfoReader::GetMipCount(256, 256) == 9);
  CHECK(BLPInfoReader::GetMipCount(256, 4) == 9); // uses the larger dimension
}

TEST_CASE("BLPInfoReader::GetMipDimensions halves per level and clamps at 1x1", "[data][blp]") {
  auto d0 = BLPInfoReader::GetMipDimensions(256, 128, 0);
  CHECK(d0.w == 256);
  CHECK(d0.h == 128);
  auto d2 = BLPInfoReader::GetMipDimensions(256, 128, 2);
  CHECK(d2.w == 64);
  CHECK(d2.h == 32);
  auto d_far = BLPInfoReader::GetMipDimensions(4, 4, 10);
  CHECK(d_far.w == 1);
  CHECK(d_far.h == 1);
}

TEST_CASE("BLPInfoReader::EstimateMemorySize sums w*h*4 across every mip level, assuming "
          "decompressed RGBA8",
          "[data][blp]") {
  BLPTextureInfo info;
  info.width = 4;
  info.height = 4;
  info.mipCount = 3; // 4x4 + 2x2 + 1x1
  const auto expected = (4u * 4u * 4u) + (2u * 2u * 4u) + (1u * 1u * 4u);
  CHECK(BLPInfoReader::EstimateMemorySize(info) == expected);
}

TEST_CASE("BLPInfoReader::EstimateMemorySize treats mipCount == 0 as a single base level",
          "[data][blp]") {
  BLPTextureInfo info;
  info.width = 8;
  info.height = 8;
  info.mipCount = 0;
  CHECK(BLPInfoReader::EstimateMemorySize(info) == 8u * 8u * 4u);
}

TEST_CASE("BLPInfoReader::GetFormatName/GetAlphaTypeName map every enum value to a distinct "
          "string",
          "[data][blp]") {
  CHECK(BLPInfoReader::GetFormatName(BLPCompression::JPEG) == "JPEG");
  CHECK(BLPInfoReader::GetFormatName(BLPCompression::Palette) == "Palette");
  CHECK(BLPInfoReader::GetFormatName(BLPCompression::DirectX) == "DirectX");
  CHECK(BLPInfoReader::GetFormatName(BLPCompression::Uncompressed) == "Uncompressed");

  CHECK(BLPInfoReader::GetAlphaTypeName(BLPAlphaType::NoAlpha) == "NoAlpha");
  CHECK(BLPInfoReader::GetAlphaTypeName(BLPAlphaType::Alpha1Bit) == "Alpha1Bit");
  CHECK(BLPInfoReader::GetAlphaTypeName(BLPAlphaType::Alpha4Bit) == "Alpha4Bit");
  CHECK(BLPInfoReader::GetAlphaTypeName(BLPAlphaType::Alpha8Bit) == "Alpha8Bit");
}

TEST_CASE("BLPInfoReader::IsPowerOfTwo classifies zero and non-powers-of-two as false",
          "[data][blp]") {
  CHECK_FALSE(BLPInfoReader::IsPowerOfTwo(0));
  CHECK(BLPInfoReader::IsPowerOfTwo(1));
  CHECK(BLPInfoReader::IsPowerOfTwo(2));
  CHECK_FALSE(BLPInfoReader::IsPowerOfTwo(3));
  CHECK(BLPInfoReader::IsPowerOfTwo(1024));
  CHECK_FALSE(BLPInfoReader::IsPowerOfTwo(1023));
}
