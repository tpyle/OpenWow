#include "openwow/network/serialization/zlib_compression.h"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

using openwow::network::serialization::CompressZlib;
using openwow::network::serialization::DecompressZlib;
using openwow::network::serialization::ZlibCompressionLevel;
using openwow::network::serialization::ZlibResult;

namespace {

std::vector<std::uint8_t> ToBytes(const std::string_view text) {
  return {text.begin(), text.end()};
}

} // namespace

TEST_CASE("CompressZlib rejects degenerate input", "[zlib]") {
  CHECK(CompressZlib(nullptr, 10).empty());
  const std::uint8_t byte = 1;
  CHECK(CompressZlib(&byte, 0).empty());
}

TEST_CASE("CompressZlib followed by DecompressZlib round-trips the original bytes", "[zlib]") {
  SECTION("highly-compressible repetitive data") {
    const std::string original(4096, 'A');
    const auto bytes = ToBytes(original);
    const auto compressed = CompressZlib(bytes.data(), bytes.size());
    REQUIRE_FALSE(compressed.empty());
    CHECK(compressed.size() < bytes.size());

    const auto decompressed = DecompressZlib(compressed.data(), compressed.size(), bytes.size());
    REQUIRE(decompressed.size() == bytes.size());
    CHECK(std::string(decompressed.begin(), decompressed.end()) == original);
  }

  SECTION("short, non-repetitive data") {
    const std::string original = "the quick brown fox jumps over the lazy dog";
    const auto bytes = ToBytes(original);
    const auto compressed = CompressZlib(bytes.data(), bytes.size());
    REQUIRE_FALSE(compressed.empty());

    const auto decompressed = DecompressZlib(compressed.data(), compressed.size(), bytes.size());
    REQUIRE(decompressed.size() == bytes.size());
    CHECK(std::string(decompressed.begin(), decompressed.end()) == original);
  }

  SECTION("single byte") {
    const std::uint8_t byte = 0x42;
    const auto compressed = CompressZlib(&byte, 1);
    REQUIRE_FALSE(compressed.empty());

    const auto decompressed = DecompressZlib(compressed.data(), compressed.size(), 1);
    REQUIRE(decompressed.size() == 1);
    CHECK(decompressed[0] == 0x42);
  }
}

TEST_CASE("CompressZlib respects the requested compression level", "[zlib]") {
  const std::string original(4096, 'B');
  const auto bytes = ToBytes(original);
  const auto fast = CompressZlib(bytes.data(), bytes.size(), ZlibCompressionLevel::kBestSpeed);
  const auto best =
      CompressZlib(bytes.data(), bytes.size(), ZlibCompressionLevel::kBestCompression);
  REQUIRE_FALSE(fast.empty());
  REQUIRE_FALSE(best.empty());
  // Both must still decompress back to the original regardless of level.
  const auto decompressed_fast = DecompressZlib(fast.data(), fast.size(), bytes.size());
  const auto decompressed_best = DecompressZlib(best.data(), best.size(), bytes.size());
  CHECK(std::string(decompressed_fast.begin(), decompressed_fast.end()) == original);
  CHECK(std::string(decompressed_best.begin(), decompressed_best.end()) == original);
}

TEST_CASE("DecompressZlib (vector overload) rejects degenerate input", "[zlib]") {
  CHECK(DecompressZlib(nullptr, 10, 10).empty());
  const std::uint8_t byte = 1;
  CHECK(DecompressZlib(&byte, 0, 10).empty());
  CHECK(DecompressZlib(&byte, 1, 0).empty());
}

TEST_CASE("DecompressZlib (vector overload) returns empty for corrupted input", "[zlib]") {
  const std::vector<std::uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF};
  CHECK(DecompressZlib(garbage.data(), garbage.size(), 1024).empty());
}

TEST_CASE("DecompressZlib (raw overload) reports kDataError for corrupted input", "[zlib]") {
  const std::vector<std::uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF};
  std::vector<std::uint8_t> output(1024);
  std::size_t output_size = output.size();
  const auto result = DecompressZlib(output.data(), &output_size, garbage.data(), garbage.size());
  CHECK(result == ZlibResult::kDataError);
}

TEST_CASE("DecompressZlib (raw overload) reports kStreamError for null arguments", "[zlib]") {
  std::uint8_t output_byte = 0;
  std::size_t output_size = 1;
  const std::uint8_t input_byte = 1;

  CHECK(DecompressZlib(nullptr, &output_size, &input_byte, 1) == ZlibResult::kStreamError);
  CHECK(DecompressZlib(&output_byte, nullptr, &input_byte, 1) == ZlibResult::kStreamError);
  CHECK(DecompressZlib(&output_byte, &output_size, nullptr, 1) == ZlibResult::kStreamError);
}

TEST_CASE("DecompressZlib (raw overload) reports a buffer error when the output is too small",
          "[zlib]") {
  const std::string original(4096, 'C');
  const auto bytes = ToBytes(original);
  const auto compressed = CompressZlib(bytes.data(), bytes.size());
  REQUIRE_FALSE(compressed.empty());

  std::vector<std::uint8_t> output(4); // far too small for 4096 bytes of 'C'
  std::size_t output_size = output.size();
  const auto result =
      DecompressZlib(output.data(), &output_size, compressed.data(), compressed.size());
  CHECK(result == ZlibResult::kBufferError);
}

TEST_CASE("IsPlausibleZlibInflatedSize bounds a declared size by deflate's maximum ratio",
          "[network][zlib]") {
  using openwow::network::serialization::IsPlausibleZlibInflatedSize;
  using openwow::network::serialization::kZlibMaxInflateRatio;
  CHECK(IsPlausibleZlibInflatedSize(10, 10 * kZlibMaxInflateRatio));
  CHECK_FALSE(IsPlausibleZlibInflatedSize(10, 10 * kZlibMaxInflateRatio + 1));
  CHECK_FALSE(IsPlausibleZlibInflatedSize(6, 0xFFFFFFFFu));
  CHECK_FALSE(IsPlausibleZlibInflatedSize(0, 1));
  CHECK(IsPlausibleZlibInflatedSize(0, 0));
  CHECK(IsPlausibleZlibInflatedSize(static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)));
}

TEST_CASE("IsPlausibleZlibInflatedSize accepts a real highly-compressible payload",
          "[network][zlib]") {
  using namespace openwow::network::serialization;
  const std::vector<std::uint8_t> zeros(1 << 20, 0);
  const auto compressed = CompressZlib(zeros.data(), zeros.size(), ZlibCompressionLevel::kBestCompression);
  REQUIRE_FALSE(compressed.empty());
  CHECK(IsPlausibleZlibInflatedSize(compressed.size(), zeros.size()));
}
