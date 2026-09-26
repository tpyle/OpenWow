#include "openwow/game/activities/lfg/rules/lfg_role_rules.h"

#include <array>

namespace openwow::game::lfg {
namespace {

// Indexed by ChrClasses id: warriors and death knights tank or deal damage,
// paladins and druids can take every role, priests and shamans heal or deal
// damage, the rest only deal damage. Every class may lead.
constexpr std::array<std::uint8_t, 13> kRoleAvailabilityMaskByClassId = {
    0x00, 0x0B, 0x0F, 0x09, 0x09, 0x0D, 0x0B,
    0x0D, 0x09, 0x09, 0x00, 0x0F, 0x00,
};

}  // namespace

std::uint8_t RoleAvailabilityMaskForClass(const std::uint8_t class_id) {
  if (class_id >= kRoleAvailabilityMaskByClassId.size()) {
    return 0;
  }
  return kRoleAvailabilityMaskByClassId[class_id];
}

std::uint8_t FilterRolesForClass(const std::uint8_t class_id,
                                 const std::uint8_t roles) {
  return static_cast<std::uint8_t>(roles & RoleAvailabilityMaskForClass(class_id));
}

}  // namespace openwow::game::lfg
