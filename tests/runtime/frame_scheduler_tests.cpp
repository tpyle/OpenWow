#include "openwow/runtime/scheduling/frame_scheduler.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

using openwow::core::CallbackHandle;
using openwow::core::FrameScheduler;
using openwow::core::Phase;

namespace {

struct SchedulerFixture {
  SchedulerFixture() { FrameScheduler::Instance().Clear(); }
  ~SchedulerFixture() { FrameScheduler::Instance().Clear(); }
  FrameScheduler& scheduler = FrameScheduler::Instance();
};

}  // namespace

TEST_CASE_METHOD(SchedulerFixture, "FrameScheduler runs phases in order and by priority",
                 "[runtime][frame_scheduler]") {
  std::vector<int> order;
  (void)scheduler.Register(Phase::Render, 0, [&](double) { order.push_back(4); });
  (void)scheduler.Register(Phase::Update, 5, [&](double) { order.push_back(3); });
  (void)scheduler.Register(Phase::Update, 1, [&](double) { order.push_back(2); });
  (void)scheduler.Register(Phase::EarlyUpdate, 9, [&](double) { order.push_back(1); });
  scheduler.RunFrame(0.016);
  CHECK(order == std::vector<int>{1, 2, 3, 4});
  CHECK(scheduler.GetFrameCount() == 1u);
}

TEST_CASE_METHOD(SchedulerFixture,
                 "FrameScheduler skips a callback unregistered earlier in the same frame",
                 "[runtime][frame_scheduler]") {
  int second_calls = 0;
  CallbackHandle second = CallbackHandle::Invalid;
  (void)scheduler.Register(Phase::Update, 0, [&](double) { scheduler.Unregister(second); });
  second = scheduler.Register(Phase::Update, 1, [&](double) { ++second_calls; });

  scheduler.RunFrame(0.0);
  CHECK(second_calls == 0);
  CHECK_FALSE(scheduler.IsRegistered(second));
  CHECK(scheduler.GetCallbackCount(Phase::Update) == 1u);
}

TEST_CASE_METHOD(SchedulerFixture, "FrameScheduler defers registration made during a frame",
                 "[runtime][frame_scheduler]") {
  int added_calls = 0;
  bool registered = false;
  (void)scheduler.Register(Phase::Update, 0, [&](double) {
    if (!registered) {
      registered = true;
      (void)scheduler.Register(Phase::Update, 1, [&](double) { ++added_calls; });
    }
  });

  scheduler.RunFrame(0.0);
  CHECK(added_calls == 0);
  scheduler.RunFrame(0.0);
  CHECK(added_calls == 1);
}

TEST_CASE_METHOD(SchedulerFixture,
                 "FrameScheduler unregistering a callback added in the same frame drops it",
                 "[runtime][frame_scheduler]") {
  int added_calls = 0;
  (void)scheduler.Register(Phase::Update, 0, [&](double) {
    const auto handle = scheduler.Register(Phase::Update, 1, [&](double) { ++added_calls; });
    CHECK(scheduler.Unregister(handle));
  });
  scheduler.RunFrame(0.0);
  scheduler.RunFrame(0.0);
  CHECK(added_calls == 0);
  CHECK(scheduler.GetCallbackCount(Phase::Update) == 1u);
}

TEST_CASE_METHOD(SchedulerFixture,
                 "FrameScheduler keeps deferring changes during a nested RunPhase",
                 "[runtime][frame_scheduler]") {
  int late_calls = 0;
  int nested_runs = 0;
  (void)scheduler.Register(Phase::LateUpdate, 0, [&](double) { ++late_calls; });
  (void)scheduler.Register(Phase::Update, 0, [&](double) {
    ++nested_runs;
    scheduler.RunPhase(Phase::LateUpdate, 0.0);
    // Still inside the outer frame: this must be deferred, not applied.
    (void)scheduler.Register(Phase::Update, 1, [](double) {});
    CHECK(scheduler.GetCallbackCount(Phase::Update) == 1u);
  });

  scheduler.RunFrame(0.0);
  CHECK(nested_runs == 1);
  CHECK(late_calls == 2);  // once nested, once in the frame's own LateUpdate
  CHECK(scheduler.GetCallbackCount(Phase::Update) == 2u);
}

TEST_CASE_METHOD(SchedulerFixture, "FrameScheduler Clear during a frame takes effect afterwards",
                 "[runtime][frame_scheduler]") {
  int later_calls = 0;
  (void)scheduler.Register(Phase::Update, 0, [&](double) { scheduler.Clear(); });
  (void)scheduler.Register(Phase::Update, 1, [&](double) { ++later_calls; });
  scheduler.RunFrame(0.0);
  CHECK(later_calls == 0);
  CHECK(scheduler.GetTotalCallbackCount() == 0u);
}

TEST_CASE_METHOD(SchedulerFixture, "FrameScheduler recovers its run state after a throwing callback",
                 "[runtime][frame_scheduler]") {
  const auto thrower =
      scheduler.Register(Phase::Update, 0, [](double) { throw std::runtime_error("boom"); });
  CHECK_THROWS(scheduler.RunFrame(0.0));
  // Not running any more, so changes apply immediately.
  CHECK(scheduler.Unregister(thrower));
  CHECK(scheduler.GetTotalCallbackCount() == 0u);
}
