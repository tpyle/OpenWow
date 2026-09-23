#include "openwow/foundation/text/utf8.h"

#include <catch2/catch_test_macros.hpp>

using namespace openwow::text;

namespace {
// "café" = c, a, f, U+00E9 (2-byte UTF-8: 0xC3 0xA9) -> 5 bytes, 4 codepoints.
constexpr const char *kCafe = "caf\xC3\xA9";
// "hi" + U+1F600 grinning-face emoji (4-byte UTF-8: F0 9F 98 80) -> 6 bytes,
// 3 codepoints.
constexpr const char *kHiEmoji = "hi\xF0\x9F\x98\x80";
} // namespace

TEST_CASE("PopBackUtf8 removes exactly one trailing codepoint, of any byte width", "[text][utf8]") {
  SECTION("ASCII: removes one byte") {
    std::string text = "hello";
    PopBackUtf8(&text);
    CHECK(text == "hell");
  }

  SECTION("2-byte codepoint: removes both bytes, not just the last one") {
    std::string text = kCafe;
    PopBackUtf8(&text);
    CHECK(text == "caf");
  }

  SECTION("4-byte codepoint: removes all four bytes") {
    std::string text = kHiEmoji;
    PopBackUtf8(&text);
    CHECK(text == "hi");
  }
}

TEST_CASE("PopBackUtf8 no-ops on null or empty input", "[text][utf8]") {
  PopBackUtf8(nullptr); // must not crash

  std::string empty;
  PopBackUtf8(&empty);
  CHECK(empty.empty());
}

TEST_CASE("Utf8CodepointCount counts codepoints, not bytes", "[text][utf8]") {
  CHECK(Utf8CodepointCount("") == 0);
  CHECK(Utf8CodepointCount("hello") == 5);
  CHECK(Utf8CodepointCount(kCafe) == 4);
  CHECK(Utf8CodepointCount(kHiEmoji) == 3);
}

TEST_CASE("ClampUtf8ByteIndex clamps out-of-range indices to the string bounds", "[text][utf8]") {
  CHECK(ClampUtf8ByteIndex(kCafe, -5) == 0);
  CHECK(ClampUtf8ByteIndex(kCafe, 1000) == static_cast<int>(std::string_view(kCafe).size()));
  CHECK(ClampUtf8ByteIndex(kCafe, 0) == 0);
}

TEST_CASE("ClampUtf8ByteIndex walks a mid-codepoint index back to the lead byte", "[text][utf8]") {
  // kCafe bytes: c(0) a(1) f(2) 0xC3(3, lead) 0xA9(4, continuation).
  CHECK(ClampUtf8ByteIndex(kCafe, 4) == 3);
  CHECK(ClampUtf8ByteIndex(kCafe, 3) == 3); // already on a lead byte
}

TEST_CASE("Utf8PrevByteIndex/Utf8NextByteIndex step across a whole codepoint at once",
          "[text][utf8]") {
  // kCafe: byte 3 is the start of the 2-byte 'e' with acute; byte 5 is end-of-string.
  CHECK(Utf8NextByteIndex(kCafe, 3) == 5);
  CHECK(Utf8PrevByteIndex(kCafe, 5) == 3);
}

TEST_CASE("Utf8NextByteIndex never steps past the end of the string, even for truncated input",
          "[text][utf8]") {
  const std::string_view truncated_emoji = "\xF0\x9F"; // first 2 of 4 bytes of an emoji
  CHECK(Utf8NextByteIndex(truncated_emoji, 0) == static_cast<int>(truncated_emoji.size()));
}

TEST_CASE("Utf8PrevByteIndex at the start of the string stays at 0", "[text][utf8]") {
  CHECK(Utf8PrevByteIndex(kCafe, 0) == 0);
}

TEST_CASE("Utf8TakeCodepoints truncates by codepoint count, not byte count", "[text][utf8]") {
  CHECK(Utf8TakeCodepoints(kCafe, 3) == "caf");
  CHECK(Utf8TakeCodepoints(kCafe, 4) == kCafe);
  CHECK(Utf8TakeCodepoints(kHiEmoji, 2) == "hi");
}

TEST_CASE("Utf8TakeCodepoints returns everything if max_codepoints exceeds the string's length",
          "[text][utf8]") {
  CHECK(Utf8TakeCodepoints(kCafe, 1000) == kCafe);
}

TEST_CASE("Utf8TakeCodepoints returns empty for a non-positive limit or empty input",
          "[text][utf8]") {
  CHECK(Utf8TakeCodepoints(kCafe, 0).empty());
  CHECK(Utf8TakeCodepoints(kCafe, -1).empty());
  CHECK(Utf8TakeCodepoints("", 10).empty());
}

TEST_CASE("AppendUtf8Clamped appends only as many codepoints as fit under the total limit",
          "[text][utf8]") {
  std::string base = "ab";             // 2 codepoints already
  AppendUtf8Clamped(&base, "cdef", 5); // room for 3 more
  CHECK(base == "abcde");
}

TEST_CASE("AppendUtf8Clamped counts codepoints, not bytes, toward the limit", "[text][utf8]") {
  std::string base = "hi";                // 2 codepoints
  AppendUtf8Clamped(&base, kHiEmoji, 3);  // room for exactly 1 more codepoint
  CHECK(base == std::string("hi") + "h"); // only the first codepoint ('h') of kHiEmoji fits
}

TEST_CASE("AppendUtf8Clamped is a no-op once the base is already at or over the limit",
          "[text][utf8]") {
  std::string base = "abc";
  AppendUtf8Clamped(&base, "more text", 3);
  CHECK(base == "abc");
}

TEST_CASE("AppendUtf8Clamped no-ops on a null base, empty addition, or non-positive limit",
          "[text][utf8]") {
  AppendUtf8Clamped(nullptr, "text", 5); // must not crash

  std::string base = "abc";
  AppendUtf8Clamped(&base, "", 10);
  CHECK(base == "abc");

  AppendUtf8Clamped(&base, "more", 0);
  CHECK(base == "abc");
}
