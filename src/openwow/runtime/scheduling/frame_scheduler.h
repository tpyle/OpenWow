
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace openwow::core {

enum class Phase : std::uint8_t {
    EarlyUpdate = 0,
    Update      = 1,
    LateUpdate  = 2,
    Render      = 3,
    PostRender  = 4,
};

inline constexpr std::size_t kPhaseCount = 5;

[[nodiscard]] constexpr std::string_view PhaseName(Phase p) noexcept {
    switch (p) {
        case Phase::EarlyUpdate: return "EarlyUpdate";
        case Phase::Update:      return "Update";
        case Phase::LateUpdate:  return "LateUpdate";
        case Phase::Render:      return "Render";
        case Phase::PostRender:  return "PostRender";
        default:                 return "[Unknown]";
    }
}

using FrameCallback = std::function<void(double delta_sec)>;

enum class CallbackHandle : std::uint64_t { Invalid = 0 };

class FrameScheduler {
public:

    static FrameScheduler& Instance();

    FrameScheduler(const FrameScheduler&) = delete;
    FrameScheduler& operator=(const FrameScheduler&) = delete;

    [[nodiscard]]
    CallbackHandle Register(Phase phase, int priority, FrameCallback callback,
                            std::string_view debug_name = {});

    bool Unregister(CallbackHandle handle);

    [[nodiscard]] bool IsRegistered(CallbackHandle handle) const;

    void RunFrame(double delta_sec);

    void RunPhase(Phase phase, double delta_sec);

    [[nodiscard]] std::size_t GetCallbackCount(Phase phase) const;

    [[nodiscard]] std::size_t GetTotalCallbackCount() const;

    [[nodiscard]] std::uint64_t GetFrameCount() const noexcept;

    void Clear();

private:
    FrameScheduler();
    ~FrameScheduler();

    struct Entry {
        CallbackHandle  handle;
        int             priority;
        FrameCallback   callback;
        std::string     debug_name;
    };

    struct PhaseData {
        std::vector<Entry> entries;
        bool               dirty{false};
    };

    void ApplyPendingLocked();
    void SortPhase(PhaseData& pd);
    void BeginRun();
    void EndRun();

    // Keeps running_depth_ balanced even if a callback throws.
    struct RunScope {
        explicit RunScope(FrameScheduler& scheduler) : scheduler_(scheduler) {
            scheduler_.BeginRun();
        }
        ~RunScope() { scheduler_.EndRun(); }
        RunScope(const RunScope&) = delete;
        RunScope& operator=(const RunScope&) = delete;
        FrameScheduler& scheduler_;
    };
    void RunPhaseEntries(PhaseData& pd, double delta_sec);
    [[nodiscard]] bool IsPendingRemoval(CallbackHandle handle) const;

    mutable std::mutex   mutex_;
    PhaseData            phases_[kPhaseCount];

    std::vector<std::pair<Phase, Entry>> pending_adds_;
    std::vector<CallbackHandle>          pending_removes_;

    std::atomic<std::uint64_t> next_handle_{1};
    std::atomic<std::uint64_t> frame_count_{0};
    // Nesting depth of RunFrame/RunPhase, guarded by mutex_. While
    // non-zero, Register/Unregister/Clear defer their changes so the
    // entries being iterated are never mutated.
    int                        running_depth_{0};
};

}
