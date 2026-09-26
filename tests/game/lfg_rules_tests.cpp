#include "openwow/game/activities/lfg/rules/lfg_dungeon_rules.h"
#include "openwow/game/activities/lfg/rules/lfg_role_rules.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

namespace lfg = openwow::game::lfg;

namespace {
constexpr std::uint8_t kLeadTankDamage = lfg::kRoleLeader | lfg::kRoleTank | lfg::kRoleDamage;
constexpr std::uint8_t kLeadHealDamage = lfg::kRoleLeader | lfg::kRoleHealer | lfg::kRoleDamage;
constexpr std::uint8_t kLeadDamage = lfg::kRoleLeader | lfg::kRoleDamage;
constexpr std::uint8_t kAllRoles = 0x0Fu;
}  // namespace

TEST_CASE("LFG role availability by class", "[lfg][rules]") {
  CHECK(lfg::RoleAvailabilityMaskForClass(0) == 0u);
  CHECK(lfg::RoleAvailabilityMaskForClass(1) == kLeadTankDamage);   // warrior
  CHECK(lfg::RoleAvailabilityMaskForClass(2) == kAllRoles);         // paladin
  CHECK(lfg::RoleAvailabilityMaskForClass(3) == kLeadDamage);       // hunter
  CHECK(lfg::RoleAvailabilityMaskForClass(4) == kLeadDamage);       // rogue
  CHECK(lfg::RoleAvailabilityMaskForClass(5) == kLeadHealDamage);   // priest
  CHECK(lfg::RoleAvailabilityMaskForClass(6) == kLeadTankDamage);   // death knight
  CHECK(lfg::RoleAvailabilityMaskForClass(7) == kLeadHealDamage);   // shaman
  CHECK(lfg::RoleAvailabilityMaskForClass(8) == kLeadDamage);       // mage
  CHECK(lfg::RoleAvailabilityMaskForClass(9) == kLeadDamage);       // warlock
  CHECK(lfg::RoleAvailabilityMaskForClass(10) == 0u);               // unused id
  CHECK(lfg::RoleAvailabilityMaskForClass(11) == kAllRoles);        // druid
  CHECK(lfg::RoleAvailabilityMaskForClass(12) == 0u);
  CHECK(lfg::RoleAvailabilityMaskForClass(13) == 0u);
  CHECK(lfg::RoleAvailabilityMaskForClass(255) == 0u);
}

TEST_CASE("LFG role filtering drops roles the class can't take", "[lfg][rules]") {
  CHECK(lfg::FilterRolesForClass(8, kAllRoles) == kLeadDamage);
  CHECK(lfg::FilterRolesForClass(1, lfg::kRoleHealer) == 0u);
  CHECK(lfg::FilterRolesForClass(5, lfg::kRoleHealer | lfg::kRoleTank) == lfg::kRoleHealer);
  CHECK(lfg::FilterRolesForClass(11, kAllRoles) == kAllRoles);
  CHECK(lfg::FilterRolesForClass(200, kAllRoles) == 0u);
  // Bits outside the four role bits never survive.
  CHECK(lfg::FilterRolesForClass(2, 0xF0u) == 0u);
}

TEST_CASE("LFG dungeon ids pack the type into the high byte", "[lfg][rules]") {
  STATIC_CHECK(lfg::PackDungeonId(258u, 6u) == 0x06000102u);
  STATIC_CHECK(lfg::PackDungeonId(0x01234567u, 1u) == 0x01234567u);  // id truncated to 24 bits
  STATIC_CHECK(lfg::DungeonSelectionType(0x05000010u) == 5u);
  STATIC_CHECK(lfg::DungeonSelectionType(lfg::PackDungeonId(42u, 2u)) == 2u);
}

TEST_CASE("LFG selection: which existing dungeons survive an addition",
          "[lfg][rules]") {
  const auto dungeon = lfg::PackDungeonId(10u, 1u);
  const auto heroic = lfg::PackDungeonId(11u, 5u);
  const auto raid = lfg::PackDungeonId(12u, 2u);
  const auto random = lfg::PackDungeonId(13u, 6u);

  SECTION("dungeons and heroics mix") {
    CHECK(lfg::CanKeepSelectedDungeon(1u, 0u, dungeon));
    CHECK(lfg::CanKeepSelectedDungeon(1u, 0u, heroic));
    CHECK(lfg::CanKeepSelectedDungeon(5u, 3u, dungeon));
    CHECK_FALSE(lfg::CanKeepSelectedDungeon(1u, 0u, raid));
    CHECK_FALSE(lfg::CanKeepSelectedDungeon(5u, 0u, random));
  }
  SECTION("raids mix only with raids, and only when solo") {
    CHECK(lfg::CanKeepSelectedDungeon(2u, 0u, raid));
    CHECK_FALSE(lfg::CanKeepSelectedDungeon(2u, 1u, raid));
    CHECK_FALSE(lfg::CanKeepSelectedDungeon(2u, 0u, dungeon));
  }
  SECTION("anything else replaces the selection") {
    CHECK_FALSE(lfg::CanKeepSelectedDungeon(6u, 0u, random));
    CHECK_FALSE(lfg::CanKeepSelectedDungeon(6u, 0u, dungeon));
    CHECK_FALSE(lfg::CanKeepSelectedDungeon(4u, 0u, lfg::PackDungeonId(14u, 4u)));
  }
}

namespace {

lfg::RandomDungeonCandidate Random(std::uint32_t id, std::uint32_t expansion,
                                   std::uint32_t target_average, std::uint32_t min_level) {
  return {.dungeon_id = id,
          .type_id = lfg::kRandomDungeonTypeId,
          .expansion_level = expansion,
          .target_level_average = target_average,
          .min_level = min_level};
}

}  // namespace

TEST_CASE("LFG random dungeon list", "[lfg][rules]") {
  const std::vector<lfg::RandomDungeonCandidate> candidates{
      Random(258, 0, 20, 15),
      {.dungeon_id = 36, .type_id = 1},                                          // plain dungeon
      {.dungeon_id = 285, .type_id = 1, .flags = lfg::kSeasonalDungeonFlag},    // seasonal
      {.dungeon_id = 286, .type_id = 1, .flags = lfg::kSeasonalDungeonFlag},    // seasonal, locked
      Random(262, 2, 80, 80),
  };
  const auto unlocked = [](std::uint32_t packed) { return packed == lfg::PackDungeonId(285, 1); };
  CHECK(lfg::AvailableRandomDungeonIds(candidates, unlocked) ==
        std::vector<std::uint32_t>{258, 285, 262});
  CHECK(lfg::AvailableRandomDungeonIds({}, unlocked).empty());
}

TEST_CASE("LFG best random dungeon", "[lfg][rules]") {
  const auto all = [](std::uint32_t) { return true; };

  SECTION("newest expansion wins") {
    const std::vector<lfg::RandomDungeonCandidate> c{Random(1, 0, 70, 60), Random(2, 2, 75, 65),
                                                     Random(3, 1, 80, 70)};
    CHECK(lfg::BestRandomDungeonId(c, all) == 2u);
  }
  SECTION("then the highest target level") {
    const std::vector<lfg::RandomDungeonCandidate> c{Random(1, 2, 75, 60), Random(2, 2, 80, 60)};
    CHECK(lfg::BestRandomDungeonId(c, all) == 2u);
  }
  SECTION("then the highest minimum level; the first wins full ties") {
    const std::vector<lfg::RandomDungeonCandidate> c{Random(1, 2, 80, 70), Random(2, 2, 80, 75),
                                                     Random(3, 2, 80, 75)};
    CHECK(lfg::BestRandomDungeonId(c, all) == 2u);
  }
  SECTION("only joinable random dungeons count") {
    const std::vector<lfg::RandomDungeonCandidate> c{
        Random(1, 0, 20, 15), Random(2, 2, 80, 80),
        {.dungeon_id = 3, .type_id = 1, .expansion_level = 2, .target_level_average = 80}};
    const auto not_two = [](std::uint32_t packed) { return packed != lfg::PackDungeonId(2, 6); };
    CHECK(lfg::BestRandomDungeonId(c, not_two) == 1u);
    CHECK_FALSE(lfg::BestRandomDungeonId(c, [](std::uint32_t) { return false; }));
  }
}
