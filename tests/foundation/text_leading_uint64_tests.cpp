#include "openwow/foundation/text/leading_uint64.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using openwow::text::ParseLeadingUInt64;

TEST_CASE("ParseLeadingUInt64 reads leading digits", "[text]") {
  CHECK(ParseLeadingUInt64("0") == 0u);
  CHECK(ParseLeadingUInt64("42") == 42u);
  CHECK(ParseLeadingUInt64("123abc") == 123u);
  CHECK(ParseLeadingUInt64("7 8") == 7u);
  CHECK(ParseLeadingUInt64("0042") == 42u);
}

TEST_CASE("ParseLeadingUInt64 without a leading digit is zero", "[text]") {
  CHECK(ParseLeadingUInt64("") == 0u);
  CHECK(ParseLeadingUInt64("abc") == 0u);
  CHECK(ParseLeadingUInt64(" 42") == 0u);
  CHECK(ParseLeadingUInt64("+42") == 0u);
  CHECK(ParseLeadingUInt64("-") == 0u);
}

TEST_CASE("ParseLeadingUInt64 negates with a leading minus", "[text]") {
  CHECK(ParseLeadingUInt64("-1") == UINT64_MAX);
  CHECK(ParseLeadingUInt64("-42") == static_cast<std::uint64_t>(-42));
  CHECK(ParseLeadingUInt64("--1") == 0u);
}

TEST_CASE("ParseLeadingUInt64 handles values past 32 bits", "[text]") {
  CHECK(ParseLeadingUInt64("4294967296") == 4294967296ull);
  CHECK(ParseLeadingUInt64("1234567890123") == 1234567890123ull);
  // A player guid as sent in calendar mail subjects.
  CHECK(ParseLeadingUInt64("72057594037927941") == 72057594037927941ull);
  CHECK(ParseLeadingUInt64("18446744073709551615") == UINT64_MAX);
}
