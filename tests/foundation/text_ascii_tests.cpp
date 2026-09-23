#include "openwow/foundation/text/ascii.h"

#include <catch2/catch_test_macros.hpp>

using namespace openwow::text;

TEST_CASE("ToLowerAsciiChar/ToUpperAsciiChar only affect ASCII letters", "[text][ascii]") {
  CHECK(ToLowerAsciiChar('A') == 'a');
  CHECK(ToLowerAsciiChar('Z') == 'z');
  CHECK(ToUpperAsciiChar('a') == 'A');
  CHECK(ToUpperAsciiChar('z') == 'Z');

  SECTION("non-letters pass through unchanged") {
    CHECK(ToLowerAsciiChar('5') == '5');
    CHECK(ToUpperAsciiChar('5') == '5');
    CHECK(ToLowerAsciiChar(' ') == ' ');
  }

  SECTION("high-bit bytes (non-ASCII) pass through unchanged, no sign-extension surprises") {
    const unsigned char high_byte = 0xE9; // e.g. Latin-1 'e' with acute
    CHECK(ToLowerAsciiChar(high_byte) == high_byte);
    CHECK(ToUpperAsciiChar(high_byte) == high_byte);
  }
}

TEST_CASE("ToLowerAscii/ToUpperAscii transform whole strings", "[text][ascii]") {
  CHECK(ToLowerAscii("Hello, World! 123") == "hello, world! 123");
  CHECK(ToUpperAscii("Hello, World! 123") == "HELLO, WORLD! 123");
  CHECK(ToLowerAscii("") == "");
}

TEST_CASE("Trim strips leading/trailing whitespace", "[text][ascii]") {
  CHECK(Trim("  hello  ") == "hello");
  CHECK(Trim("\t\r\nhello\r\n\t") == "hello");
  CHECK(Trim("no-trim-needed") == "no-trim-needed");
  CHECK(Trim("  inner   spaces  kept  ") == "inner   spaces  kept");
}

TEST_CASE("Trim on an all-whitespace string returns an empty string, not the original",
          "[text][ascii]") {
  CHECK(Trim("   \t\r\n  ") == "");
  CHECK(Trim("") == "");
}

TEST_CASE("StripUtf8Bom removes a leading BOM if present", "[text][ascii]") {
  const std::string with_bom = "\xEF\xBB\xBFhello";
  CHECK(StripUtf8Bom(with_bom) == "hello");
}

TEST_CASE("StripUtf8Bom leaves input without a BOM untouched", "[text][ascii]") {
  CHECK(StripUtf8Bom("hello") == "hello");
  CHECK(StripUtf8Bom("") == "");
}

TEST_CASE("StripUtf8Bom does not read out of bounds on short input", "[text][ascii]") {
  CHECK(StripUtf8Bom("\xEF\xBB") == "\xEF\xBB"); // only 2 of the 3 BOM bytes
  CHECK(StripUtf8Bom("\xEF") == "\xEF");
}

TEST_CASE("EqualsIgnoreCaseAscii compares case-insensitively", "[text][ascii]") {
  CHECK(EqualsIgnoreCaseAscii("Hello", "hello"));
  CHECK(EqualsIgnoreCaseAscii("HELLO", "hello"));
  CHECK_FALSE(EqualsIgnoreCaseAscii("Hello", "World"));
  CHECK_FALSE(EqualsIgnoreCaseAscii("Hello", "Hello!")); // different length
}

TEST_CASE(
    "EqualsIgnoreCaseAscii (const char*) rejects null pointers rather than treating them as equal",
    "[text][ascii]") {
  CHECK_FALSE(EqualsIgnoreCaseAscii(static_cast<const char *>(nullptr),
                                    static_cast<const char *>(nullptr)));
  CHECK_FALSE(EqualsIgnoreCaseAscii("hello", static_cast<const char *>(nullptr)));
  CHECK_FALSE(EqualsIgnoreCaseAscii(static_cast<const char *>(nullptr), "hello"));
  CHECK(EqualsIgnoreCaseAscii("hello", "HELLO"));
}
