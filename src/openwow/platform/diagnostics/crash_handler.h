#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace openwow::platform {

struct RingBufferEntry {
  std::chrono::steady_clock::time_point timestamp;
  std::string message;
  std::uint8_t severity{0};
};

class ErrorRingBuffer {
 public:
  static constexpr std::size_t kDefaultCapacity = 256;

  explicit ErrorRingBuffer(std::size_t capacity = kDefaultCapacity);

  void Push(std::string_view message, std::uint8_t severity);

  [[nodiscard]] std::vector<RingBufferEntry> Snapshot() const;

  [[nodiscard]] std::size_t Size() const;

  void Clear();

  [[nodiscard]] std::size_t Capacity() const { return capacity_; }

 private:
  mutable std::mutex mutex_;
  std::vector<RingBufferEntry> buffer_;
  std::size_t head_{0};
  std::size_t count_{0};
  std::size_t capacity_;
};

struct CrashContext {
  std::string build_version;
  std::string gpu_info;
  std::string active_state;
  std::string realm_name;
  std::string realm_type;
  std::string current_map;
  std::string logs_directory = "Logs";
};

class CrashHandler {
 public:
  static CrashHandler& Get();

  void Install(const CrashContext& context = {});

  void Uninstall();

  [[nodiscard]] bool IsInstalled() const { return installed_; }

  [[nodiscard]] ErrorRingBuffer& GetRingBuffer() { return ring_buffer_; }
  [[nodiscard]] const ErrorRingBuffer& GetRingBuffer() const { return ring_buffer_; }

  void SetBuildVersion(std::string_view version);
  void SetGpuInfo(std::string_view info);
  void SetActiveState(std::string_view state);
  void SetRealmInfo(std::string_view realm_name, std::string_view realm_type);
  void ClearRealmInfo();
  void SetCurrentMap(std::string_view map);

  [[nodiscard]] CrashContext GetContext() const;

  // Builds/writes the full narrative crash report (GPU info, active state,
  // realm, recent log ring buffer, ...). NOT async-signal-safe: both
  // allocate (std::string/std::ostringstream) and touch the filesystem via
  // std::filesystem/std::ofstream, and WriteCrashReport() also locks
  // mutex_. Only call these from ordinary (non-signal-handler) code; the
  // POSIX signal handler uses WriteMinimalCrashReport() instead.
  [[nodiscard]] std::string FormatCrashReport(
      std::string_view signal_info,
      const std::vector<std::string>& stack) const;

  [[nodiscard]] std::string WriteCrashReport(
      std::string_view signal_info,
      const std::vector<std::string>& stack) const;

  [[nodiscard]] static std::vector<std::string> CaptureStackTrace(
      int max_frames = 64);

  [[nodiscard]] static std::string SignalName(int signal_number);

  // Async-signal-safe: writes a minimal crash report (signal name + raw
  // backtrace) using only write()/open()/backtrace_symbols_fd() -- no
  // allocation, no locking. This is the only CrashHandler entry point the
  // POSIX signal handler may call. A signal delivered via abort() runs on
  // the thread that raised it, which -- if the abort came from glibc's
  // malloc() detecting heap corruption -- is still holding malloc's
  // internal arena lock; any malloc()/new/std::string/std::filesystem call
  // from here would try to re-acquire that same non-recursive lock and
  // deadlock the process against itself permanently (observable as an
  // unkillable hang that only SIGKILL/-9 can end, since the process never
  // reaches the point of actually dying). No-op on Windows, where the crash
  // path goes through CrashExceptionFilter/minidump instead.
  void WriteMinimalCrashReport(int signal_number,
                                const void* fault_address) const noexcept;

  using PreCrashCallback = std::function<void()>;
  void SetPreCrashCallback(PreCrashCallback cb);

  void Reset();

 private:
  CrashHandler() = default;

  mutable std::mutex mutex_;
  CrashContext context_;
  ErrorRingBuffer ring_buffer_;
  bool installed_{false};
  PreCrashCallback pre_crash_callback_;

  // Snapshot of context_.logs_directory taken at Install() time (a plain
  // NUL-terminated buffer, not std::string), so WriteMinimalCrashReport()
  // can open a crash log file without locking mutex_ or allocating.
  static constexpr std::size_t kRawLogsDirCapacity = 512;
  char raw_logs_dir_[kRawLogsDirCapacity]{};
};

inline void CrashLog(std::string_view msg, std::uint8_t severity = 0) {
  CrashHandler::Get().GetRingBuffer().Push(msg, severity);
}

}
