#pragma once

#include <cmath>
#include <cstdint>

namespace openwow::text {

/// Parses the decimal digits at the start of `str` into a 64-bit value, the
/// way the client's string library does: an optional leading '-' negates
/// the result (two's complement), parsing stops at the first non-digit, and
/// a string that doesn't start with a digit yields 0. There is no overflow
/// check. Digits are accumulated in 32-bit runs that are folded into the
/// result with a power of ten, so very long inputs round like the original.
/// `str` must be non-null and NUL-terminated.
[[nodiscard]] inline std::uint64_t ParseLeadingUInt64(const char* str) {
  const char* p = str;
  bool negative = false;
  if (*p == '-') {
    negative = true;
    ++p;
  }

  const auto digit_at = [](const char* c) {
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(*c)) - 48u;
  };
  if (digit_at(p) >= 10u) {
    return 0;
  }

  std::uint64_t high = 0;
  std::uint32_t low = digit_at(p);
  const char* run_start = p;
  ++p;

  while (digit_at(p) < 10u) {
    low = digit_at(p) + 10u * low;
    ++p;

    // Fold before the next digit could overflow the 32-bit run.
    if (low >= 0x19999999u) {
      const auto run_length = static_cast<std::uint32_t>(p - run_start);
      const double scale = std::pow(10.0, static_cast<double>(run_length));
      high = low + static_cast<std::uint64_t>(scale + 0.5) * high;
      low = 0;
      run_start = p;
    }
  }

  std::uint64_t result = low;
  if (high != 0) {
    const auto run_length = static_cast<std::uint32_t>(p - run_start);
    const double scale = std::pow(10.0, static_cast<double>(run_length));
    result = low + static_cast<std::uint64_t>(scale + 0.5) * high;
  }

  return negative ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(result)) : result;
}

}  // namespace openwow::text
