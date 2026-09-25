#include "openwow/render/resources/textures/texture_cache_budget.h"

#include <catch2/catch_test_macros.hpp>

using openwow::render::kTextureCacheEvictionsPerSweep;
using openwow::render::kTextureCacheMaxEntries;
using openwow::render::kTextureCacheUrgentEvictionsPerSweep;
using openwow::render::TextureCacheEvictionQuota;
using openwow::render::TextureCacheOverBudget;

TEST_CASE("Texture cache evicts when over its byte budget", "[render][texture_cache]") {
  CHECK(TextureCacheOverBudget(300u, 256u, 10u));
  CHECK_FALSE(TextureCacheOverBudget(256u, 256u, 10u));
}

TEST_CASE("Texture cache evicts when over its entry cap even within bytes",
          "[render][texture_cache]") {
  CHECK(TextureCacheOverBudget(1u, 256u, kTextureCacheMaxEntries + 1u));
  CHECK_FALSE(TextureCacheOverBudget(1u, 256u, kTextureCacheMaxEntries));
  // The cap stays below bgfx's default texture handle pool.
  STATIC_REQUIRE(kTextureCacheMaxEntries < 4096u);
}

TEST_CASE("Eviction quota doesn't treat under-budget usage as urgent",
          "[render][texture_cache]") {
  // Under the byte budget (evicting only for the entry cap) must not wrap
  // around into the urgent quota.
  CHECK(TextureCacheEvictionQuota(10u, 256u) == kTextureCacheEvictionsPerSweep);
  CHECK(TextureCacheEvictionQuota(300u, 256u) == kTextureCacheEvictionsPerSweep);
  CHECK(TextureCacheEvictionQuota(600u, 256u) == kTextureCacheUrgentEvictionsPerSweep);
}
