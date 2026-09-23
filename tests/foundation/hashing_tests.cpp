#include "openwow/foundation/hashing/retail_adler_seed.h"
#include "openwow/foundation/hashing/retail_sha1.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using Catch::Approx;
using namespace openwow::foundation::hashing;

namespace {

std::string Sha1Hex(const char *text) {
  RetailSha1State state;
  InitializeRetailSha1(state);
  UpdateRetailSha1(state, text);
  std::uint8_t digest[20] = {};
  FinalizeRetailSha1(state, digest);

  std::string hex;
  hex.reserve(40);
  char buf[3];
  for (const std::uint8_t byte : digest) {
    std::snprintf(buf, sizeof(buf), "%02x", byte);
    hex += buf;
  }
  return hex;
}

} // namespace

// --- retail_sha1.h -- a standard, unmodified SHA-1; real known-answer
// vectors apply directly. ---------------------------------------------

TEST_CASE("RetailSha1State has the expected on-wire layout", "[hashing][sha1]") {
  // 4 (bit_count_low) + 4 (bit_count_high) + 20 (digest) + 64 (buffer).
  STATIC_REQUIRE(sizeof(RetailSha1State) == 92);
}

TEST_CASE("SHA-1 of the empty string matches the standard known-answer vector", "[hashing][sha1]") {
  CHECK(Sha1Hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST_CASE("SHA-1 of \"abc\" matches the standard known-answer vector", "[hashing][sha1]") {
  CHECK(Sha1Hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST_CASE("SHA-1 of a pangram matches the standard known-answer vector", "[hashing][sha1]") {
  CHECK(Sha1Hex("The quick brown fox jumps over the lazy dog") ==
        "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

TEST_CASE("SHA-1 matches whether the message is fed in one call or several", "[hashing][sha1]") {
  const std::string message = "The quick brown fox jumps over the lazy dog";

  RetailSha1State single_call;
  InitializeRetailSha1(single_call);
  UpdateRetailSha1(single_call, reinterpret_cast<const std::uint8_t *>(message.data()),
                   static_cast<std::uint32_t>(message.size()));
  std::uint8_t single_digest[20] = {};
  FinalizeRetailSha1(single_call, single_digest);

  RetailSha1State chunked;
  InitializeRetailSha1(chunked);
  const std::string first_half = message.substr(0, 9); // "The quick"
  const std::string second_half = message.substr(9);
  UpdateRetailSha1(chunked, reinterpret_cast<const std::uint8_t *>(first_half.data()),
                   static_cast<std::uint32_t>(first_half.size()));
  UpdateRetailSha1(chunked, reinterpret_cast<const std::uint8_t *>(second_half.data()),
                   static_cast<std::uint32_t>(second_half.size()));
  std::uint8_t chunked_digest[20] = {};
  FinalizeRetailSha1(chunked, chunked_digest);

  CHECK(std::memcmp(single_digest, chunked_digest, sizeof(single_digest)) == 0);
}

TEST_CASE("SHA-1 handles a message that is an exact multiple of the 64-byte block size",
          "[hashing][sha1]") {
  const std::string message(128, 'x'); // exactly two full blocks
  RetailSha1State state;
  InitializeRetailSha1(state);
  UpdateRetailSha1(state, reinterpret_cast<const std::uint8_t *>(message.data()),
                   static_cast<std::uint32_t>(message.size()));
  std::uint8_t digest[20] = {};
  FinalizeRetailSha1(state, digest);
  // Just confirm this doesn't crash/corrupt and produces a non-zero
  // digest; the known-answer vectors above already validate correctness
  // of the core transform.
  bool all_zero = true;
  for (const std::uint8_t byte : digest) {
    if (byte != 0)
      all_zero = false;
  }
  CHECK_FALSE(all_zero);
}

// --- retail_adler_seed.h -- NOT Adler-32 despite the name; a from-scratch
// reimplementation of the original client's own PRNG, with no external
// public reference to compute known-answer vectors from. These tests
// therefore check structural/statistical properties (determinism, output
// ranges) rather than pinning specific output values. If real-client
// output for a given seed is ever obtained, upgrading these to exact
// known-answer vectors would be a meaningfully stronger test. -----------

TEST_CASE("AdvanceAdlerSeed is deterministic for a given seed", "[hashing][adler_seed]") {
  AdlerSeedState a = MakeAdlerSeedState(12345);
  AdlerSeedState b = MakeAdlerSeedState(12345);

  for (int i = 0; i < 8; ++i) {
    CAPTURE(i);
    CHECK(AdvanceAdlerSeed(a) == AdvanceAdlerSeed(b));
  }
}

TEST_CASE("AdvanceAdlerSeed produces a changing sequence, not a constant or fixed-point value",
          "[hashing][adler_seed]") {
  AdlerSeedState state = MakeAdlerSeedState(1);
  const std::uint32_t first = AdvanceAdlerSeed(state);
  const std::uint32_t second = AdvanceAdlerSeed(state);
  const std::uint32_t third = AdvanceAdlerSeed(state);
  CHECK(first != second);
  CHECK(second != third);
}

TEST_CASE("Different seeds produce different sequences", "[hashing][adler_seed]") {
  AdlerSeedState a = MakeAdlerSeedState(1);
  AdlerSeedState b = MakeAdlerSeedState(2);
  CHECK(AdvanceAdlerSeed(a) != AdvanceAdlerSeed(b));
}

TEST_CASE("AdlerSeedNextUnitFloat stays within [0, 1)", "[hashing][adler_seed]") {
  AdlerSeedState state = MakeAdlerSeedState(42);
  for (int i = 0; i < 256; ++i) {
    const float value = AdlerSeedNextUnitFloat(state);
    CAPTURE(i, value);
    CHECK(value >= 0.0f);
    CHECK(value < 1.0f);
  }
}

TEST_CASE("AdlerSeedNextSignedUnitFloat stays within [-1, 1]", "[hashing][adler_seed]") {
  AdlerSeedState state = MakeAdlerSeedState(7);
  for (int i = 0; i < 256; ++i) {
    const float value = AdlerSeedNextSignedUnitFloat(state);
    CAPTURE(i, value);
    CHECK(value >= -1.0f);
    CHECK(value <= 1.0f);
  }
}

TEST_CASE("AdlerSeedNextRangeFloat stays within the requested [lower, upper] range",
          "[hashing][adler_seed]") {
  AdlerSeedState state = MakeAdlerSeedState(99);
  for (int i = 0; i < 256; ++i) {
    const float value = AdlerSeedNextRangeFloat(10.0f, 20.0f, state);
    CAPTURE(i, value);
    CHECK(value >= 10.0f);
    CHECK(value <= 20.0f);
  }
}

TEST_CASE("AdlerSeedNextBoundedValue stays strictly below the requested upper bound",
          "[hashing][adler_seed]") {
  AdlerSeedState state = MakeAdlerSeedState(1234);
  for (int i = 0; i < 256; ++i) {
    const std::uint32_t value = AdlerSeedNextBoundedValue(17, state);
    CAPTURE(i, value);
    CHECK(value < 17);
  }
}

TEST_CASE("AdlerSeedNextUnitCircleDirection produces a unit-length direction vector",
          "[hashing][adler_seed]") {
  AdlerSeedState state = MakeAdlerSeedState(555);
  for (int i = 0; i < 64; ++i) {
    const auto direction = AdlerSeedNextUnitCircleDirection(state);
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    CAPTURE(i, direction.x, direction.y, length);
    CHECK(length == Approx(1.0f).margin(0.001));
  }
}
