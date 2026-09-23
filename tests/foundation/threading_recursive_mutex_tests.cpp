#include "openwow/foundation/threading/recursive_mutex.h"

#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using openwow::foundation::RecursiveMutex;

TEST_CASE("RecursiveMutex: a single thread can lock and unlock without contention",
          "[foundation][recursive_mutex]") {
  RecursiveMutex mutex;
  mutex.lock();
  mutex.unlock();
  CHECK(mutex.try_lock());
  mutex.unlock();
}

TEST_CASE("RecursiveMutex: the owning thread can re-enter the lock, and must unlock the "
          "same number of times",
          "[foundation][recursive_mutex]") {
  RecursiveMutex mutex;
  mutex.lock();
  mutex.lock(); // re-entrant: would deadlock on a plain std::mutex
  mutex.lock();
  CHECK(mutex.try_lock()); // also re-entrant

  mutex.unlock();
  mutex.unlock();
  mutex.unlock();
  mutex.unlock();

  // Fully released: a fresh acquisition (e.g. from a different logical
  // caller) must succeed rather than still behaving as if still held.
  CHECK(mutex.try_lock());
  mutex.unlock();
}

TEST_CASE("RecursiveMutex::try_lock fails (without blocking) when another thread holds the lock",
          "[foundation][recursive_mutex]") {
  RecursiveMutex mutex;
  mutex.lock();

  bool other_thread_try_lock_succeeded = true;
  std::thread worker([&] { other_thread_try_lock_succeeded = mutex.try_lock(); });
  worker.join();

  CHECK_FALSE(other_thread_try_lock_succeeded);
  mutex.unlock();

  // Now that the owner released it, a different thread's try_lock should succeed.
  bool second_attempt_succeeded = false;
  std::thread worker2([&] {
    second_attempt_succeeded = mutex.try_lock();
    if (second_attempt_succeeded)
      mutex.unlock();
  });
  worker2.join();
  CHECK(second_attempt_succeeded);
}

TEST_CASE("RecursiveMutex actually excludes concurrent access across threads",
          "[foundation][recursive_mutex]") {
  RecursiveMutex mutex;
  int shared_counter = 0; // deliberately non-atomic: correctness depends on the mutex
  constexpr int kThreadCount = 8;
  constexpr int kIncrementsPerThread = 2000;

  std::vector<std::thread> threads;
  threads.reserve(kThreadCount);
  for (int t = 0; t < kThreadCount; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < kIncrementsPerThread; ++i) {
        mutex.lock();
        ++shared_counter;
        mutex.unlock();
      }
    });
  }
  for (std::thread &thread : threads)
    thread.join();

  CHECK(shared_counter == kThreadCount * kIncrementsPerThread);
}

TEST_CASE("RecursiveMutex: nested locking from within a held lock is still mutually exclusive "
          "with other threads",
          "[foundation][recursive_mutex]") {
  RecursiveMutex mutex;
  int shared_counter = 0;
  constexpr int kThreadCount = 4;
  constexpr int kIterationsPerThread = 500;

  std::vector<std::thread> threads;
  threads.reserve(kThreadCount);
  for (int t = 0; t < kThreadCount; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < kIterationsPerThread; ++i) {
        mutex.lock();
        mutex.lock(); // re-entrant nested acquisition while under contention
        ++shared_counter;
        mutex.unlock();
        mutex.unlock();
      }
    });
  }
  for (std::thread &thread : threads)
    thread.join();

  CHECK(shared_counter == kThreadCount * kIterationsPerThread);
}
