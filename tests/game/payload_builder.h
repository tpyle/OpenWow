#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace openwow::test {

/// Builds little-endian packet payloads for decoder tests.
class PayloadBuilder {
 public:
  PayloadBuilder& u8(std::uint8_t v) {
    data_.push_back(v);
    return *this;
  }
  PayloadBuilder& u32(std::uint32_t v) { return raw(&v, 4); }
  PayloadBuilder& i32(std::int32_t v) { return raw(&v, 4); }
  PayloadBuilder& u64(std::uint64_t v) { return raw(&v, 8); }
  /// A NUL-terminated string.
  PayloadBuilder& str(std::string_view s) {
    data_.insert(data_.end(), s.begin(), s.end());
    data_.push_back(0);
    return *this;
  }
  [[nodiscard]] std::span<const std::uint8_t> span() const { return data_; }
  /// The payload with its last `n` bytes cut off.
  [[nodiscard]] std::span<const std::uint8_t> truncated(std::size_t n = 1) const {
    return span().first(data_.size() - n);
  }
  [[nodiscard]] const std::uint8_t* data() const { return data_.data(); }
  [[nodiscard]] std::size_t size() const { return data_.size(); }

 private:
  PayloadBuilder& raw(const void* p, std::size_t n) {
    const auto* b = static_cast<const std::uint8_t*>(p);
    data_.insert(data_.end(), b, b + n);
    return *this;
  }
  std::vector<std::uint8_t> data_;
};

}  // namespace openwow::test
