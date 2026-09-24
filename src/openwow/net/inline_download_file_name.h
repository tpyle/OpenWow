#pragma once

#include <string_view>

namespace openwow::net {

/// Returns true when a server-supplied inline-download file name is a single
/// plain file-name component that stays inside the client directory once it
/// is joined onto it.
///
/// The auth server names the file for a login patch transfer, and the client
/// writes (and replaces) that file under its install directory. Names that
/// contain a path separator, a drive/stream colon, a control character, or
/// that are "." / ".." are rejected so a hostile server cannot direct the
/// write outside that directory.
[[nodiscard]] constexpr bool IsSafeInlineDownloadFileName(std::string_view name) noexcept {
  if (name.empty() || name == "." || name == "..") {
    return false;
  }
  for (const char ch : name) {
    const auto byte = static_cast<unsigned char>(ch);
    if (byte < 0x20u || byte == 0x7Fu || ch == '/' || ch == '\\' || ch == ':') {
      return false;
    }
  }
  return true;
}

}  // namespace openwow::net
