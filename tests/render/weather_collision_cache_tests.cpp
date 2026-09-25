#include "openwow/render/world/environment/weather_collision_cache.h"

#include <catch2/catch_test_macros.hpp>

using openwow::render::WeatherCollisionCache;

TEST_CASE("Weather collision cache shares a surface within a cell", "[render][weather]") {
  WeatherCollisionCache cache(1.0f, 2.0);
  cache.Store(10.2f, -3.7f, 0.0, {.hit = true, .z = 55.0f, .normal = {0.0f, 0.0f, 1.0f}});
  const auto same_cell = cache.Find(10.9f, -3.1f, 0.5);
  REQUIRE(same_cell.has_value());
  CHECK(same_cell->hit);
  CHECK(same_cell->z == 55.0f);
  CHECK_FALSE(cache.Find(11.1f, -3.1f, 0.5).has_value());
  CHECK_FALSE(cache.Find(10.5f, -4.1f, 0.5).has_value());
}

TEST_CASE("Weather collision cache remembers misses", "[render][weather]") {
  WeatherCollisionCache cache;
  cache.Store(0.5f, 0.5f, 0.0, {.hit = false});
  const auto entry = cache.Find(0.5f, 0.5f, 1.0);
  REQUIRE(entry.has_value());
  CHECK_FALSE(entry->hit);
}

TEST_CASE("Weather collision cache entries expire", "[render][weather]") {
  WeatherCollisionCache cache(1.0f, 2.0);
  cache.Store(-0.5f, -0.5f, 0.0, {.hit = true, .z = 1.0f});
  CHECK(cache.Find(-0.5f, -0.5f, 2.0).has_value());
  CHECK_FALSE(cache.Find(-0.5f, -0.5f, 2.5).has_value());
  cache.Store(5.0f, 5.0f, 2.0, {.hit = true, .z = 2.0f});
  cache.Prune(3.0);
  CHECK(cache.size() == 1u);
  CHECK(cache.Find(5.0f, 5.0f, 3.0).has_value());
}

TEST_CASE("Weather collision cache keeps negative cells distinct", "[render][weather]") {
  WeatherCollisionCache cache;
  cache.Store(-0.1f, 0.1f, 0.0, {.hit = true, .z = 1.0f});
  cache.Store(0.1f, -0.1f, 0.0, {.hit = true, .z = 2.0f});
  CHECK(cache.Find(-0.2f, 0.2f, 0.0)->z == 1.0f);
  CHECK(cache.Find(0.2f, -0.2f, 0.0)->z == 2.0f);
  CHECK_FALSE(cache.Find(0.1f, 0.1f, 0.0).has_value());
}
