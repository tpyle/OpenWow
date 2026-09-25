#pragma once

#include <array>
#include <cstdint>

/// Pure decision rules for keeping locally cached account data in step with
/// the server's copy. Kept free of I/O so they can be unit tested.
namespace openwow::game::account_data_sync {

/// Copies server timestamps into `synchronized` only for the slots whose bit
/// is set in `mask`. SMSG_ACCOUNT_DATA_TIMES is sent once for the
/// account-wide slots (at character select) and again for the per-character
/// slots (at world entry); a slot the packet doesn't cover keeps its last
/// known server timestamp.
inline void ApplyServerTimesForMask(
    std::array<std::uint32_t, 8>& synchronized,
    const std::array<std::uint32_t, 8>& incoming, const std::uint32_t mask) {
  for (std::size_t slot = 0; slot < synchronized.size(); ++slot) {
    if ((mask & (1u << slot)) != 0u) {
      synchronized[slot] = incoming[slot];
    }
  }
}

/// Timestamps of one slot as held in memory: `timestamp` is the time of the
/// held payload, `synchronized_timestamp` the server's latest known time.
struct SlotTimes {
  std::uint32_t timestamp = 0;
  std::uint32_t synchronized_timestamp = 0;
};

/// What to do with an account-wide slot's pre-existing in-memory state when
/// the on-disk cache is loaded over it at world entry.
enum class DiskLoadMerge {
  /// The disk copy is at least as new; use it unchanged.
  kKeepDisk,
  /// Memory holds a payload downloaded from the server that is newer than
  /// the disk copy; adopt it (and persist it).
  kAdoptServerPayload,
  /// The server reported a newer time but its payload hasn't arrived yet;
  /// keep the disk payload but remember the server's time, so the slot is
  /// requested from the server instead of uploaded over it.
  kAdoptServerTime,
};

/// Chooses how to merge `memory` (state received from the server before the
/// disk cache could be read) with `disk` (state just loaded from the cache).
/// A downloaded payload is recognisable by its payload time equalling the
/// server time.
inline DiskLoadMerge ChooseDiskLoadMerge(const SlotTimes& memory,
                                         const SlotTimes& disk) {
  if (memory.timestamp != 0u &&
      memory.timestamp == memory.synchronized_timestamp &&
      memory.timestamp > disk.timestamp) {
    return DiskLoadMerge::kAdoptServerPayload;
  }
  if (memory.synchronized_timestamp > disk.synchronized_timestamp) {
    return DiskLoadMerge::kAdoptServerTime;
  }
  return DiskLoadMerge::kKeepDisk;
}

}  // namespace openwow::game::account_data_sync
