#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openwow::network::serialization {

enum class ZlibCompressionLevel : int {
  kDefault = -1,
  kBestSpeed = 1,
  kBestCompression = 9,
};

enum class ZlibResult : int {
  kOk = 0,
  kNeedDictionary = 2,
  kStreamError = -2,
  kDataError = -3,
  kMemoryError = -4,
  kBufferError = -5,
  kVersionError = -6,
};

/// Upper bound on how much a deflate stream can expand (about 1032:1 for a
/// maximal run). A declared uncompressed size above compressed_size times
/// this ratio cannot be genuine.
inline constexpr std::size_t kZlibMaxInflateRatio = 1032;

/// Returns true when `declared_size` bytes could really come out of
/// `compressed_size` bytes of zlib data. Callers that size an output buffer
/// from a peer-declared length use this before allocating, so a tiny packet
/// cannot demand a multi-gigabyte buffer.
[[nodiscard]] constexpr bool IsPlausibleZlibInflatedSize(std::size_t compressed_size,
                                                         std::size_t declared_size) noexcept {
  if (compressed_size > static_cast<std::size_t>(-1) / kZlibMaxInflateRatio) {
    return true;
  }
  return declared_size <= compressed_size * kZlibMaxInflateRatio;
}

[[nodiscard]] std::vector<std::uint8_t> CompressZlib(
    const std::uint8_t* data, std::size_t size,
    ZlibCompressionLevel level = ZlibCompressionLevel::kDefault);

[[nodiscard]] ZlibResult DecompressZlib(std::uint8_t* output,
                                        std::size_t* output_size,
                                        const std::uint8_t* input,
                                        std::size_t input_size);

[[nodiscard]] std::vector<std::uint8_t> DecompressZlib(
    const std::uint8_t* data, std::size_t size,
    std::size_t max_output_size);

}
