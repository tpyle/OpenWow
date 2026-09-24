#include "openwow/net/inline_download_file_name.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

using openwow::net::IsSafeInlineDownloadFileName;

TEST_CASE("IsSafeInlineDownloadFileName accepts plain file names", "[net][inline_download]") {
  CHECK(IsSafeInlineDownloadFileName("Patch"));
  CHECK(IsSafeInlineDownloadFileName("wow-patch.mpq"));
  CHECK(IsSafeInlineDownloadFileName("Survey"));
  CHECK(IsSafeInlineDownloadFileName("...hidden"));
  CHECK(IsSafeInlineDownloadFileName("name with spaces.bin"));
}

TEST_CASE("IsSafeInlineDownloadFileName rejects names that escape the client directory",
          "[net][inline_download]") {
  CHECK_FALSE(IsSafeInlineDownloadFileName(""));
  CHECK_FALSE(IsSafeInlineDownloadFileName("."));
  CHECK_FALSE(IsSafeInlineDownloadFileName(".."));
  CHECK_FALSE(IsSafeInlineDownloadFileName("../../.bashrc"));
  CHECK_FALSE(IsSafeInlineDownloadFileName("..\\..\\evil.exe"));
  CHECK_FALSE(IsSafeInlineDownloadFileName("/home/user/.config/autostart/x.desktop"));
  CHECK_FALSE(IsSafeInlineDownloadFileName("Data/patch-4.mpq"));
  CHECK_FALSE(IsSafeInlineDownloadFileName("C:evil"));
  CHECK_FALSE(IsSafeInlineDownloadFileName("file.txt:stream"));
}

TEST_CASE("IsSafeInlineDownloadFileName rejects control characters", "[net][inline_download]") {
  CHECK_FALSE(IsSafeInlineDownloadFileName(std::string("a\0b", 3)));
  CHECK_FALSE(IsSafeInlineDownloadFileName("a\nb"));
  CHECK_FALSE(IsSafeInlineDownloadFileName("a\x7F"));
}

static_assert(IsSafeInlineDownloadFileName("Patch"));
static_assert(!IsSafeInlineDownloadFileName("../x"));
