#pragma once

#include <cstdint>

/// Which Dungeon Finder roles each class may queue as.
namespace openwow::game::lfg {

/// Role bits as sent in CMSG_LFG_JOIN / SetLFGRoles.
inline constexpr std::uint8_t kRoleLeader = 0x01u;
inline constexpr std::uint8_t kRoleTank = 0x02u;
inline constexpr std::uint8_t kRoleHealer = 0x04u;
inline constexpr std::uint8_t kRoleDamage = 0x08u;

/// The role bits `class_id` (ChrClasses id) can take; 0 for an unknown class.
[[nodiscard]] std::uint8_t RoleAvailabilityMaskForClass(std::uint8_t class_id);

/// `roles` restricted to those `class_id` can take.
[[nodiscard]] std::uint8_t FilterRolesForClass(std::uint8_t class_id,
                                               std::uint8_t roles);

}  // namespace openwow::game::lfg
