#include "openwow/game/lfg_search_context.h"

#include "openwow/core/storm_string.h"
#include "openwow/data/formats/dbc/dbc_loader.h"
#include "openwow/game/unit_query_bridge.h"

namespace openwow::game {
namespace {

constexpr std::size_t kSearchSortStringLimit = 0x7FFFFFFFu;

std::optional<UnitQuerySnapshot> FindSearchPlayer(WorldSession *session,
                                                  const std::uint64_t guid) {
  if (session == nullptr || guid == 0) {
    return std::nullopt;
  }
  return UnitQueryBridge::Get().GetPlayerInfoByGUID(session, guid);
}

}  // namespace

lfg::SearchSortContext MakeLfgSearchSortContext(WorldSession *session,
                                                const openwow::data::dbc::DbcLoader *dbc) {
  lfg::SearchSortContext context;
  context.find_player =
      [session](const std::uint64_t guid) -> std::optional<lfg::SearchPlayerIdentity> {
    const auto snapshot = FindSearchPlayer(session, guid);
    if (!snapshot.has_value()) {
      return std::nullopt;
    }
    return lfg::SearchPlayerIdentity{.name = snapshot->name, .class_id = snapshot->classId};
  };
  context.area_name = [dbc](const std::uint32_t area_id) -> std::optional<std::string_view> {
    const auto *area = dbc != nullptr ? dbc->area_table().LookupEntry(area_id) : nullptr;
    if (area == nullptr) {
      return std::nullopt;
    }
    return area->name;
  };
  context.class_name = [dbc](const std::uint8_t class_id) -> std::optional<std::string_view> {
    const auto *chr_class = dbc != nullptr ? dbc->chr_classes().LookupEntry(class_id) : nullptr;
    if (chr_class == nullptr) {
      return std::nullopt;
    }
    return chr_class->name;
  };
  context.compare_labels = [](const char *left, const char *right) {
    return openwow::core::SStrCmpNoCaseCollate(left, right, kSearchSortStringLimit);
  };
  context.compare_player_names = [](const char *left, const char *right) {
    return openwow::core::SStrCmpI(left, right, kSearchSortStringLimit);
  };
  return context;
}

bool IsLfgSearchPlayerKnown(WorldSession *session, const std::uint64_t guid) {
  return FindSearchPlayer(session, guid).has_value();
}

}  // namespace openwow::game
