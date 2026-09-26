#include "openwow/game/activities/lfg/model/lfg_server_messages.h"

#include <algorithm>
#include <cstring>

namespace openwow::game {

void LfgUpdateInfo::AddDungeonSelection(const std::uint32_t packed_dungeon_id) {
  if (ContainsDungeonSelection(packed_dungeon_id)) {
    return;
  }

  dungeons.push_back(packed_dungeon_id);
}

bool LfgUpdateInfo::ContainsDungeonSelection(const std::uint32_t packed_dungeon_id) const {
  return std::find(dungeons.begin(), dungeons.end(), packed_dungeon_id) != dungeons.end();
}

void LfgUpdateInfo::SyncFrom(const LfgUpdateInfo& src) {

  has_extra = src.has_extra;
  joined = src.joined;
  queued = src.queued;
  raw_flag_4 = src.raw_flag_4;
  raw_flag_5 = src.raw_flag_5;
  raw_tail_bytes = src.raw_tail_bytes;

  comment = src.comment;

  dungeons.erase(
      std::remove_if(dungeons.begin(), dungeons.end(),
                     [&src](const std::uint32_t id) {
                       return !src.ContainsDungeonSelection(id);
                     }),
      dungeons.end());

  for (const std::uint32_t id : src.dungeons) {
    AddDungeonSelection(id);
  }
}

bool LfgUpdateInfo::MatchesServerSnapshot(const LfgUpdateInfo& other) const {
  if (has_extra != other.has_extra || joined != other.joined || queued != other.queued ||
      raw_flag_4 != other.raw_flag_4 || raw_flag_5 != other.raw_flag_5 ||
      raw_tail_bytes != other.raw_tail_bytes || dungeons.size() != other.dungeons.size()) {
    return false;
  }

  // Case-sensitive, up to the first NUL (decoded comments never contain
  // one).
  if (std::strcmp(comment.c_str(), other.comment.c_str()) != 0) {
    return false;
  }

  return std::all_of(
      dungeons.begin(), dungeons.end(),
      [&other](const std::uint32_t packed_dungeon_id) {
        return other.ContainsDungeonSelection(packed_dungeon_id);
      });
}

}  // namespace openwow::game
