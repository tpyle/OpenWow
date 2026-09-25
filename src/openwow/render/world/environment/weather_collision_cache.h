#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <unordered_map>

namespace openwow::render {

/// Remembers where falling weather particles land, per horizontal cell of
/// their spawn position.
///
/// Every particle in a weather system falls in nearly the same direction, so
/// particles spawned in the same small cell land on the same surface. Casting
/// one collision ray per cell instead of one per particle turns tens of
/// thousands of rays a second into a few hundred. Entries expire so a change
/// of wind or of nearby geometry is picked up.
class WeatherCollisionCache {
 public:
  struct Surface {
    /// False when the ray found nothing within range.
    bool hit{false};
    float z{0.0f};
    std::array<float, 3> normal{0.0f, 0.0f, 1.0f};
  };

  explicit WeatherCollisionCache(float cell_size = 1.0f, double lifetime_seconds = 2.0)
      : cell_size_(cell_size), lifetime_seconds_(lifetime_seconds) {}

  /// The surface recorded for the cell containing (x, y), if still fresh at `now`.
  [[nodiscard]] std::optional<Surface> Find(float x, float y, double now) const {
    const auto it = cells_.find(KeyFor(x, y));
    if (it == cells_.end() || now - it->second.stored_at > lifetime_seconds_) {
      return std::nullopt;
    }
    return it->second.surface;
  }

  void Store(float x, float y, double now, const Surface& surface) {
    cells_[KeyFor(x, y)] = Entry{surface, now};
  }

  /// Drops expired entries; call occasionally to bound memory.
  void Prune(double now) {
    for (auto it = cells_.begin(); it != cells_.end();) {
      it = now - it->second.stored_at > lifetime_seconds_ ? cells_.erase(it) : std::next(it);
    }
  }

  void Clear() { cells_.clear(); }

  [[nodiscard]] std::size_t size() const { return cells_.size(); }

 private:
  struct Entry {
    Surface surface;
    double stored_at{0.0};
  };

  [[nodiscard]] std::uint64_t KeyFor(float x, float y) const {
    const auto cell_x = static_cast<std::int32_t>(std::floor(x / cell_size_));
    const auto cell_y = static_cast<std::int32_t>(std::floor(y / cell_size_));
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x)) << 32u) |
           static_cast<std::uint32_t>(cell_y);
  }

  float cell_size_;
  double lifetime_seconds_;
  std::unordered_map<std::uint64_t, Entry> cells_;
};

}  // namespace openwow::render
