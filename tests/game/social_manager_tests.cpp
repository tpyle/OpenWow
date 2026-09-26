#include "openwow/game/social/contacts/application/social_manager.h"

#include "payload_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <vector>

using openwow::game::ContactInfo;
using openwow::game::FriendStatus;
using openwow::game::ObjectGuid;
using openwow::game::SocialFlag;
using openwow::game::SocialManager;
using Bytes = openwow::test::PayloadBuilder;

namespace {

constexpr std::uint32_t kFriend = 0x1;
constexpr std::uint32_t kIgnore = 0x2;
constexpr std::uint32_t kMute = 0x4;

SocialManager MakeManager() {
  return SocialManager([](const char* l, const char* r) { return std::strcmp(l, r); });
}

bool Feed(bool (SocialManager::*handler)(const std::uint8_t*, std::size_t),
          SocialManager& social, const Bytes& bytes) {
  return (social.*handler)(bytes.data(), bytes.size());
}

Bytes& OnlineFriend(Bytes& b, std::uint64_t guid, const char* note = "") {
  return b.u64(guid).u32(kFriend).str(note).u8(1).u32(12).u32(80).u32(1);
}
Bytes& OfflineFriend(Bytes& b, std::uint64_t guid) {
  return b.u64(guid).u32(kFriend).str("").u8(0);
}
Bytes& Plain(Bytes& b, std::uint64_t guid, std::uint32_t flags) {
  return b.u64(guid).u32(flags).str("");
}

std::vector<std::uint64_t> Guids(const std::vector<const ContactInfo*>& list) {
  std::vector<std::uint64_t> guids;
  for (const auto* c : list) guids.push_back(c->guid.GetRawValue());
  return guids;
}

}  // namespace

TEST_CASE("SocialManager contact list builds the three lists", "[contacts][manager]") {
  auto social = MakeManager();
  Bytes b;
  b.u32(kFriend | kIgnore | kMute).u32(4);
  OnlineFriend(b, 1, "tank");
  OfflineFriend(b, 2);
  Plain(b, 3, kIgnore);
  Plain(b, 4, kIgnore | kMute);
  REQUIRE(Feed(&SocialManager::HandleContactList, social, b));

  CHECK(social.contact_count() == 4u);
  CHECK(social.IsFriend(ObjectGuid(1)));
  CHECK(social.FindContact(ObjectGuid(1))->note == "tank");
  CHECK(social.FindContact(ObjectGuid(1))->level == 80u);
  CHECK(social.IsIgnored(ObjectGuid(4)));
  CHECK(social.IsMuted(ObjectGuid(4)));
  CHECK(social.IsIgnoredOrMuted(ObjectGuid(3)));
  CHECK_FALSE(social.IsIgnoredOrMuted(ObjectGuid(1)));
  CHECK_FALSE(social.IsMuted(ObjectGuid()));
}

TEST_CASE("SocialManager contact list replaces only the requested lists", "[contacts][manager]") {
  auto social = MakeManager();
  {
    Bytes b;
    b.u32(kFriend | kIgnore).u32(2);
    OnlineFriend(b, 1);
    Plain(b, 2, kIgnore);
    REQUIRE(Feed(&SocialManager::HandleContactList, social, b));
  }
  // A friends-only list without friend 1 drops it but keeps the ignore.
  {
    Bytes b;
    b.u32(kFriend).u32(1);
    OfflineFriend(b, 5);
    REQUIRE(Feed(&SocialManager::HandleContactList, social, b));
  }
  CHECK_FALSE(social.HasContact(ObjectGuid(1)));
  CHECK(social.IsIgnored(ObjectGuid(2)));
  CHECK(social.IsFriend(ObjectGuid(5)));
}

TEST_CASE("SocialManager contact list honours the list capacities", "[contacts][manager]") {
  auto social = MakeManager();
  Bytes b;
  b.u32(kIgnore).u32(SocialManager::kIgnoreLimit + 3);
  for (std::uint64_t guid = 1; guid <= SocialManager::kIgnoreLimit + 3; ++guid) Plain(b, guid, kIgnore);
  REQUIRE(Feed(&SocialManager::HandleContactList, social, b));
  CHECK(social.GetIgnored().size() == SocialManager::kIgnoreLimit);
  CHECK_FALSE(social.HasContact(ObjectGuid(SocialManager::kIgnoreLimit + 1)));
}

TEST_CASE("SocialManager leaves its lists alone when a packet doesn't decode",
          "[contacts][manager]") {
  auto social = MakeManager();
  Bytes b;
  b.u32(kFriend).u32(1);
  OnlineFriend(b, 1);
  REQUIRE(Feed(&SocialManager::HandleContactList, social, b));

  Bytes bad;
  bad.u32(kFriend).u32(1).u64(9).u32(kFriend).str("").u8(1);  // presence cut short
  CHECK_FALSE(Feed(&SocialManager::HandleContactList, social, bad));
  CHECK(social.IsFriend(ObjectGuid(1)));
  CHECK_FALSE(social.HasContact(ObjectGuid(9)));

  CHECK_FALSE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x03).u32(1)));
  CHECK(social.FindContact(ObjectGuid(1))->status == FriendStatus::kOnline);
  CHECK_FALSE(social.last_friend_status_update());
}

TEST_CASE("SocialManager friend status transitions", "[contacts][manager]") {
  auto social = MakeManager();

  // Added (offline), then online, then offline, then removed.
  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x07).u64(1).str("pal")));
  CHECK(social.IsFriend(ObjectGuid(1)));
  CHECK(social.FindContact(ObjectGuid(1))->note == "pal");
  REQUIRE(social.last_friend_status_update());
  CHECK(social.last_friend_status_update()->guid == ObjectGuid(1));

  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social,
               Bytes().u8(0x02).u64(1).u8(1).u32(12).u32(70).u32(8)));
  CHECK(social.FindContact(ObjectGuid(1))->player_class == 8u);

  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x1A).u64(1).u8(2)));
  CHECK(social.FindContact(ObjectGuid(1))->status == FriendStatus::kAfk);
  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x1B).u64(1).u32(4395)));
  CHECK(social.FindContact(ObjectGuid(1))->area == 4395u);

  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x03).u64(1)));
  CHECK(social.FindContact(ObjectGuid(1))->status == FriendStatus::kOffline);
  CHECK(social.FindContact(ObjectGuid(1))->area == 0u);

  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x05).u64(1)));
  CHECK_FALSE(social.HasContact(ObjectGuid(1)));

  // Ignore and mute add/remove; a contact with no list left is dropped.
  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x0F).u64(2)));
  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x16).u64(2)));
  CHECK(social.IsIgnored(ObjectGuid(2)));
  CHECK(social.IsMuted(ObjectGuid(2)));
  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x10).u64(2)));
  CHECK(social.HasContact(ObjectGuid(2)));
  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x17).u64(2)));
  CHECK_FALSE(social.HasContact(ObjectGuid(2)));

  // Results without list effects only record the update.
  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social, Bytes().u8(0x04).u64(3)));
  CHECK_FALSE(social.HasContact(ObjectGuid(3)));
  CHECK(social.last_friend_status_update()->guid == ObjectGuid(3));
}

TEST_CASE("SocialManager display order", "[contacts][manager]") {
  auto social = MakeManager();
  Bytes b;
  b.u32(kFriend | kIgnore).u32(5);
  OfflineFriend(b, 1);
  OnlineFriend(b, 2);
  OfflineFriend(b, 3);
  OnlineFriend(b, 4);
  OfflineFriend(b, 5);
  REQUIRE(Feed(&SocialManager::HandleContactList, social, b));
  REQUIRE(social.SetDisplayName(ObjectGuid(1), "carl"));
  REQUIRE(social.SetDisplayName(ObjectGuid(2), "zed"));
  REQUIRE(social.SetDisplayName(ObjectGuid(3), "anna"));
  REQUIRE(social.SetDisplayName(ObjectGuid(4), "bob"));
  // 5 has no name yet.
  CHECK_FALSE(social.SetDisplayName(ObjectGuid(99), "nobody"));

  // Online first, then by the injected name order, unnamed last.
  CHECK(Guids(social.GetFriends()) == std::vector<std::uint64_t>{4, 2, 3, 1, 5});
}

TEST_CASE("SocialManager pending deletes hide contacts and selection follows guids",
          "[contacts][manager]") {
  auto social = MakeManager();
  Bytes b;
  b.u32(kFriend).u32(2);
  OfflineFriend(b, 1);
  OfflineFriend(b, 2);
  REQUIRE(Feed(&SocialManager::HandleContactList, social, b));
  REQUIRE(social.SetDisplayName(ObjectGuid(1), "a"));
  REQUIRE(social.SetDisplayName(ObjectGuid(2), "b"));

  social.SelectFriendByLuaIndex(2);
  CHECK(social.GetSelectedFriendLuaIndex() == 2);
  social.SelectFriendByLuaIndex(0);  // out of range clears
  CHECK(social.GetSelectedFriendLuaIndex() == 0);

  social.SelectFriendByLuaIndex(2);
  REQUIRE(social.MarkDeletePending(ObjectGuid(1), SocialFlag::kFriend));
  CHECK(social.IsDeletePending(ObjectGuid(1), SocialFlag::kFriend));
  CHECK_FALSE(social.IsFriend(ObjectGuid(1)));
  CHECK(Guids(social.GetFriends()) == std::vector<std::uint64_t>{2});
  CHECK(social.GetSelectedFriendLuaIndex() == 1);  // same guid, new position
}

TEST_CASE("SocialManager complaints mute players when enabled", "[contacts][manager]") {
  auto social = MakeManager();
  social.RememberRecentComplaintGuid(7);
  CHECK(social.HasRecentComplaintGuid(7));
  CHECK_FALSE(social.IsMuted(ObjectGuid(7)));
  social.SetComplaintStatus(SocialManager::kComplaintStatusEnabled);
  CHECK(social.IsMuted(ObjectGuid(7)));

  for (std::uint64_t guid = 100; guid < 100 + SocialManager::kRecentComplaintLimit; ++guid) {
    social.RememberRecentComplaintGuid(guid);
  }
  CHECK_FALSE(social.HasRecentComplaintGuid(7));  // oldest dropped
  social.RememberRecentComplaintGuid(0);
  CHECK_FALSE(social.HasRecentComplaintGuid(0));
}

TEST_CASE("SocialManager clears the Refer-a-Friend status bit", "[contacts][manager]") {
  auto social = MakeManager();
  REQUIRE(Feed(&SocialManager::HandleFriendStatus, social,
               Bytes().u8(0x06).u64(1).str("").u8(0x01 | 0x08).u32(1).u32(1).u32(1)));
  CHECK(social.ClearFriendReferAFriendFlag(ObjectGuid(1)));
  CHECK(social.FindContact(ObjectGuid(1))->status == FriendStatus::kOnline);
  CHECK_FALSE(social.ClearFriendReferAFriendFlag(ObjectGuid(1)));
}
