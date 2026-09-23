#include "openwow/foundation/container/flat_hash_map.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

using openwow::foundation::FlatHashMap;
using openwow::foundation::StableFlatHashMap;

TEST_CASE("FlatHashMap starts empty", "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> map;
  CHECK(map.empty());
  CHECK(map.size() == 0);
  CHECK_FALSE(map.contains(1));
  CHECK(map.find(1) == map.end());
  CHECK(map.FindValue(1) == nullptr);
}

TEST_CASE("FlatHashMap try_emplace inserts a new key and reports which happened",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> map;

  const auto [it1, inserted1] = map.try_emplace(1, "one");
  CHECK(inserted1);
  CHECK(it1->first == 1);
  CHECK(it1->second == "one");
  CHECK(map.size() == 1);

  const auto [it2, inserted2] = map.try_emplace(1, "one-again");
  CHECK_FALSE(inserted2);
  CHECK(it2->second == "one"); // try_emplace does not overwrite an existing value
  CHECK(map.size() == 1);
}

TEST_CASE("FlatHashMap insert_or_assign overwrites an existing value",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> map;
  map.try_emplace(1, "one");

  const auto [it, inserted] = map.insert_or_assign(1, "ONE");
  CHECK_FALSE(inserted);
  CHECK(it->second == "ONE");
  CHECK(map.size() == 1);
}

TEST_CASE("FlatHashMap operator[] default-constructs the value for a missing key",
          "[container][flat_hash_map]") {
  FlatHashMap<int, int> map;
  CHECK(map[42] == 0); // int's default value
  CHECK(map.size() == 1);
  map[42] = 100;
  CHECK(map[42] == 100);
  CHECK(map.size() == 1); // still just the one key
}

TEST_CASE("FlatHashMap find/contains/FindValue/count agree on presence",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> map;
  map.try_emplace(1, "one");

  CHECK(map.contains(1));
  CHECK_FALSE(map.contains(2));
  CHECK(map.count(1) == 1);
  CHECK(map.count(2) == 0);
  REQUIRE(map.find(1) != map.end());
  CHECK(map.find(1)->second == "one");
  CHECK(map.find(2) == map.end());
  REQUIRE(map.FindValue(1) != nullptr);
  CHECK(*map.FindValue(1) == "one");
  CHECK(map.FindValue(2) == nullptr);
}

TEST_CASE("FlatHashMap erase removes a key and reports how many were removed",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> map;
  map.try_emplace(1, "one");
  map.try_emplace(2, "two");

  CHECK(map.erase(1) == 1);
  CHECK_FALSE(map.contains(1));
  CHECK(map.contains(2));
  CHECK(map.size() == 1);
  CHECK(map.erase(1) == 0); // already gone
}

TEST_CASE("FlatHashMap erase by iterator removes the pointed-to entry",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> map;
  map.try_emplace(1, "one");
  map.try_emplace(2, "two");

  const auto it = map.find(1);
  REQUIRE(it != map.end());
  map.erase(it);
  CHECK_FALSE(map.contains(1));
  CHECK(map.contains(2));
  CHECK(map.size() == 1);
}

TEST_CASE("FlatHashMap Clear removes every entry and resets size to zero",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> map;
  for (int i = 0; i < 5; ++i) {
    map.try_emplace(i, "value");
  }
  REQUIRE(map.size() == 5);

  map.Clear();
  CHECK(map.empty());
  CHECK(map.size() == 0);
  for (int i = 0; i < 5; ++i) {
    CHECK_FALSE(map.contains(i));
  }
}

TEST_CASE("FlatHashMap iteration visits every inserted entry exactly once",
          "[container][flat_hash_map]") {
  FlatHashMap<int, int> map;
  for (int i = 0; i < 10; ++i) {
    map.try_emplace(i, i * i);
  }

  std::size_t visited = 0;
  int sum_of_values = 0;
  for (const auto &entry : map) {
    ++visited;
    sum_of_values += entry.second;
    CHECK(entry.second == entry.first * entry.first);
  }
  CHECK(visited == 10);
  CHECK(sum_of_values == 0 + 1 + 4 + 9 + 16 + 25 + 36 + 49 + 64 + 81);
}

TEST_CASE("FlatHashMap grows to accommodate more entries than the default minimum capacity",
          "[container][flat_hash_map]") {
  // Default minimum capacity is 8, with a 75% max load factor (i.e. it
  // must grow once more than 6 entries are live). Insert well past that
  // to force at least one Resize and confirm every key survives it.
  FlatHashMap<int, int> map;
  constexpr int kCount = 50;
  for (int i = 0; i < kCount; ++i) {
    const auto [it, inserted] = map.try_emplace(i, i);
    CHECK(inserted);
  }

  CHECK(map.size() == static_cast<std::size_t>(kCount));
  CHECK(map.capacity() > 8);
  for (int i = 0; i < kCount; ++i) {
    CAPTURE(i);
    REQUIRE(map.contains(i));
    CHECK(*map.FindValue(i) == i);
  }
}

TEST_CASE("FlatHashMap correctly reuses erased slots across repeated erase/reinsert cycles",
          "[container][flat_hash_map]") {
  // Exercises the kControlErased/first_erased reuse path in PrepareInsert:
  // insert enough to force growth, erase everything, then reinsert the
  // same count again. If erased tombstones weren't counted correctly
  // (size_ + erased_) this could spuriously grow forever or corrupt state.
  FlatHashMap<int, int> map;
  constexpr int kCount = 20;
  for (int i = 0; i < kCount; ++i) {
    map.try_emplace(i, i);
  }
  const std::size_t capacity_after_first_fill = map.capacity();

  for (int i = 0; i < kCount; ++i) {
    CHECK(map.erase(i) == 1);
  }
  CHECK(map.empty());

  for (int i = 100; i < 100 + kCount; ++i) {
    const auto [it, inserted] = map.try_emplace(i, i);
    CHECK(inserted);
  }
  CHECK(map.size() == static_cast<std::size_t>(kCount));
  for (int i = 100; i < 100 + kCount; ++i) {
    CAPTURE(i);
    CHECK(map.contains(i));
  }
  // Capacity should not need to exceed what the first fill already needed.
  CHECK(map.capacity() <= capacity_after_first_fill * 2);
}

TEST_CASE("FlatHashMap copy construction is a deep, independent copy",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> original;
  original.try_emplace(1, "one");
  original.try_emplace(2, "two");

  FlatHashMap<int, std::string> copy(original);
  CHECK(copy.size() == 2);
  CHECK(*copy.FindValue(1) == "one");

  copy.insert_or_assign(1, "ONE-CHANGED");
  copy.try_emplace(3, "three");

  CHECK(*original.FindValue(1) == "one"); // original untouched
  CHECK_FALSE(original.contains(3));
  CHECK(*copy.FindValue(1) == "ONE-CHANGED");
  CHECK(copy.contains(3));
}

TEST_CASE("FlatHashMap move construction transfers ownership and empties the source",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> original;
  original.try_emplace(1, "one");
  original.try_emplace(2, "two");

  FlatHashMap<int, std::string> moved(std::move(original));
  CHECK(moved.size() == 2);
  CHECK(*moved.FindValue(1) == "one");
  CHECK(original.empty()); // moved-from is left valid but empty
}

TEST_CASE("FlatHashMap copy/move assignment replace the target's prior contents",
          "[container][flat_hash_map]") {
  FlatHashMap<int, std::string> a;
  a.try_emplace(1, "one");

  FlatHashMap<int, std::string> b;
  b.try_emplace(99, "ninety-nine");

  SECTION("copy assignment") {
    b = a;
    CHECK(b.size() == 1);
    CHECK(b.contains(1));
    CHECK_FALSE(b.contains(99));
  }

  SECTION("move assignment") {
    b = std::move(a);
    CHECK(b.size() == 1);
    CHECK(b.contains(1));
    CHECK_FALSE(b.contains(99));
  }
}

// --- StableFlatHashMap ---------------------------------------------------

TEST_CASE("StableFlatHashMap supports the same basic CRUD operations as FlatHashMap",
          "[container][stable_flat_hash_map]") {
  StableFlatHashMap<int, std::string> map;
  CHECK(map.empty());

  map.try_emplace(1, "one");
  CHECK(map.size() == 1);
  CHECK(map.contains(1));
  CHECK(*map.FindValue(1) == "one");

  map.insert_or_assign(1, "ONE");
  CHECK(*map.FindValue(1) == "ONE");

  CHECK(map.erase(1) == 1);
  CHECK(map.empty());
}

TEST_CASE("StableFlatHashMap keeps value addresses stable across growth that would resize a plain "
          "FlatHashMap",
          "[container][stable_flat_hash_map]") {
  // This is the entire reason StableFlatHashMap exists over FlatHashMap:
  // a plain FlatHashMap relocates every element's storage on Resize, so
  // taking a value's address and then inserting enough more entries to
  // force a grow would invalidate it. StableFlatHashMap must not.
  StableFlatHashMap<int, std::string> map;
  map.try_emplace(0, "zero");
  const std::string *const stable_address = map.FindValue(0);
  REQUIRE(stable_address != nullptr);

  for (int i = 1; i < 200; ++i) {
    map.try_emplace(i, "value");
  }

  CHECK(map.size() == 200);
  CHECK(map.FindValue(0) == stable_address);
  CHECK(*stable_address == "zero");
}

TEST_CASE("StableFlatHashMap reuses freed node slots after erase",
          "[container][stable_flat_hash_map]") {
  StableFlatHashMap<int, int> map;
  map.try_emplace(1, 1);
  map.try_emplace(2, 2);
  CHECK(map.erase(1) == 1);
  map.try_emplace(3, 3);

  CHECK(map.size() == 2);
  CHECK_FALSE(map.contains(1));
  CHECK(map.contains(2));
  CHECK(map.contains(3));
}

TEST_CASE("StableFlatHashMap iteration visits every entry exactly once",
          "[container][stable_flat_hash_map]") {
  StableFlatHashMap<int, int> map;
  for (int i = 0; i < 10; ++i) {
    map.try_emplace(i, i);
  }

  std::size_t visited = 0;
  for (const auto &entry : map) {
    ++visited;
    CHECK(entry.second == entry.first);
  }
  CHECK(visited == 10);
}

TEST_CASE("StableFlatHashMap copy construction is a deep, independent copy",
          "[container][stable_flat_hash_map]") {
  StableFlatHashMap<int, std::string> original;
  original.try_emplace(1, "one");

  StableFlatHashMap<int, std::string> copy(original);
  copy.insert_or_assign(1, "changed");

  CHECK(*original.FindValue(1) == "one");
  CHECK(*copy.FindValue(1) == "changed");
}

TEST_CASE("StableFlatHashMap move construction transfers ownership and empties the source",
          "[container][stable_flat_hash_map]") {
  StableFlatHashMap<int, std::string> original;
  original.try_emplace(1, "one");

  StableFlatHashMap<int, std::string> moved(std::move(original));
  CHECK(moved.size() == 1);
  CHECK(*moved.FindValue(1) == "one");
  CHECK(original.empty());
}
