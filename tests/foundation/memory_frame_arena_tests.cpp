#include "openwow/foundation/memory/frame_arena.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>

using openwow::foundation::ArenaPtr;
using openwow::foundation::ArenaSpan;
using openwow::foundation::ArenaVector;
using openwow::foundation::FrameArena;
using openwow::foundation::FrameArenaAllocator;
using openwow::foundation::FrameArenaMarker;
using openwow::foundation::FrameArenaScope;

TEST_CASE("FrameArena::Allocate returns non-null, correctly aligned, non-overlapping memory",
          "[foundation][frame_arena]") {
  FrameArena arena;
  void *a = arena.Allocate(64, 16);
  void *b = arena.Allocate(64, 16);
  REQUIRE(a != nullptr);
  REQUIRE(b != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(a) % 16 == 0);
  CHECK(reinterpret_cast<std::uintptr_t>(b) % 16 == 0);

  const auto a_begin = reinterpret_cast<std::uintptr_t>(a);
  const auto b_begin = reinterpret_cast<std::uintptr_t>(b);
  // The two 64-byte regions must not overlap.
  CHECK((a_begin + 64 <= b_begin || b_begin + 64 <= a_begin));
}

TEST_CASE("FrameArena::Allocate tolerates a zero-byte request without crashing",
          "[foundation][frame_arena]") {
  FrameArena arena;
  void *a = arena.Allocate(0, 8);
  CHECK(a != nullptr);
}

TEST_CASE("FrameArena grows into a new block once the current one is exhausted",
          "[foundation][frame_arena]") {
  // The constructor clamps any requested block size below 1024 bytes back
  // up to 1024 (kMinBlockBytes), so 1024 is the smallest usable size for
  // exercising block-growth behavior directly.
  FrameArena arena(1024);
  CHECK(arena.block_count() == 0);

  void *first = arena.Allocate(700, 8);
  REQUIRE(first != nullptr);
  CHECK(arena.block_count() == 1);

  // 700 + 700 exceeds the 1024-byte block, so this must spill into a
  // freshly allocated second block rather than fail or corrupt memory.
  void *second = arena.Allocate(700, 8);
  REQUIRE(second != nullptr);
  CHECK(arena.block_count() == 2);
  CHECK(first != second);
}

TEST_CASE("FrameArena::Mark/RewindTo deterministically reuses the same memory",
          "[foundation][frame_arena]") {
  FrameArena arena;
  (void)arena.Allocate(100, 8);
  const auto marker = arena.Mark();

  void *before_rewind = arena.Allocate(50, 8);
  CHECK(arena.bytes_in_use() >= 150);

  arena.RewindTo(marker);
  CHECK(arena.bytes_in_use() == 100);

  void *after_rewind = arena.Allocate(50, 8);
  CHECK(after_rewind == before_rewind);
}

TEST_CASE("FrameArena::Reset returns bytes_in_use to zero and reuses block 0",
          "[foundation][frame_arena]") {
  FrameArena arena;
  void *first = arena.Allocate(64, 8);
  arena.Reset();
  CHECK(arena.bytes_in_use() == 0);

  void *after_reset = arena.Allocate(64, 8);
  CHECK(after_reset == first);
}

TEST_CASE("FrameArena::generation() increments on RewindTo/Reset/ReleaseBlocks",
          "[foundation][frame_arena]") {
  FrameArena arena;
  const auto initial_generation = arena.generation();

  (void)arena.Allocate(16, 8);
  arena.Reset();
  CHECK(arena.generation() == initial_generation + 1);

  (void)arena.Allocate(16, 8);
  arena.RewindTo(FrameArenaMarker{});
  CHECK(arena.generation() == initial_generation + 2);

  arena.ReleaseBlocks();
  CHECK(arena.generation() == initial_generation + 3);
  CHECK(arena.block_count() == 0);
}

TEST_CASE("FrameArena::high_water_bytes tracks the peak, not the current, usage",
          "[foundation][frame_arena]") {
  FrameArena arena;
  (void)arena.Allocate(200, 8);
  const auto marker = arena.Mark();
  (void)arena.Allocate(300, 8);
  CHECK(arena.high_water_bytes() >= 500);

  arena.RewindTo(marker);
  CHECK(arena.bytes_in_use() < arena.high_water_bytes()); // usage dropped, high water mark did not
}

TEST_CASE("FrameArenaScope automatically rewinds the arena when it goes out of scope",
          "[foundation][frame_arena]") {
  FrameArena arena;
  (void)arena.Allocate(64, 8);
  const std::size_t bytes_before_scope = arena.bytes_in_use();

  {
    FrameArenaScope scope(arena);
    (void)scope.arena().Allocate(128, 8);
    CHECK(arena.bytes_in_use() > bytes_before_scope);
  }

  CHECK(arena.bytes_in_use() == bytes_before_scope);
}

namespace {
struct Point {
  int x = 0;
  int y = 0;
};
} // namespace

TEST_CASE("FrameArena::Create placement-constructs a trivially-destructible object",
          "[foundation][frame_arena]") {
  FrameArena arena;
  Point *p = arena.Create<Point>(3, 4);
  REQUIRE(p != nullptr);
  CHECK(p->x == 3);
  CHECK(p->y == 4);
}

TEST_CASE("FrameArena::CreateArray fills every element with the given value",
          "[foundation][frame_arena]") {
  FrameArena arena;
  int *data = arena.CreateArray<int>(5, 7);
  REQUIRE(data != nullptr);
  for (std::size_t i = 0; i < 5; ++i) {
    CAPTURE(i);
    CHECK(data[i] == 7);
  }
}

TEST_CASE("ArenaPtr dereferences to the pointed-to value while the arena's generation is unchanged",
          "[foundation][frame_arena]") {
  FrameArena arena;
  int *raw = arena.Create<int>(42);
  ArenaPtr<int> ptr(arena, raw);
  CHECK(static_cast<bool>(ptr));
  CHECK(*ptr == 42);
  CHECK(ptr.get() == raw);

  ArenaPtr<int> null_ptr;
  CHECK_FALSE(static_cast<bool>(null_ptr));
}

TEST_CASE("ArenaSpan exposes size/indexing/iteration over an arena-backed array",
          "[foundation][frame_arena]") {
  FrameArena arena;
  int *data = arena.CreateArray<int>(3, 0);
  data[0] = 10;
  data[1] = 20;
  data[2] = 30;
  ArenaSpan<int> span(arena, data, 3);

  CHECK(span.size() == 3);
  CHECK_FALSE(span.empty());
  CHECK(span[0] == 10);
  CHECK(span[1] == 20);
  CHECK(span[2] == 30);

  int sum = 0;
  for (int value : span)
    sum += value;
  CHECK(sum == 60);
}

TEST_CASE("ArenaVector (std::vector backed by FrameArenaAllocator) grows and holds values normally",
          "[foundation][frame_arena]") {
  // deallocate() on FrameArenaAllocator is a deliberate no-op -- memory a
  // growing std::vector abandons during reallocation is simply orphaned
  // until the whole arena is rewound/reset in bulk. That's expected for a
  // frame allocator, not a leak in the conventional sense.
  FrameArena arena;
  FrameArenaAllocator<int> allocator(arena);
  ArenaVector<int> vec(allocator);
  for (int i = 0; i < 50; ++i) {
    vec.push_back(i);
  }

  REQUIRE(vec.size() == 50);
  for (int i = 0; i < 50; ++i) {
    CAPTURE(i);
    CHECK(vec[static_cast<std::size_t>(i)] == i);
  }
}

TEST_CASE("FrameArenaAllocator equality reflects sharing the same underlying arena",
          "[foundation][frame_arena]") {
  FrameArena arena_a;
  FrameArena arena_b;
  FrameArenaAllocator<int> alloc_a1(arena_a);
  FrameArenaAllocator<int> alloc_a2(arena_a);
  FrameArenaAllocator<int> alloc_b(arena_b);

  CHECK(alloc_a1 == alloc_a2);
  CHECK(alloc_a1 != alloc_b);
}
