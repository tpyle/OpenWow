#include "openwow/game/activities/lfg/application/lfg_system.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <vector>

using openwow::game::LFGBootVote;
using openwow::game::LFGProposal;
using openwow::game::LFGState;
using openwow::game::LFGSystem;

namespace {

// LFGSystem is a process-wide singleton; every test starts from Reset().
LFGSystem& Fresh() {
  auto& lfg = LFGSystem::Get();
  lfg.Reset();
  return lfg;
}

constexpr std::uint32_t kDungeon = 0x01000010u;
constexpr std::uint32_t kHeroic = 0x05000011u;
constexpr std::uint32_t kRaid = 0x02000012u;
constexpr std::uint32_t kRandom = 0x06000013u;

}  // namespace

TEST_CASE("LFGSystem dungeon selection keeps compatible types", "[lfg][system]") {
  auto& lfg = Fresh();
  lfg.AddSelectedDungeon(kDungeon, 0u);
  lfg.AddSelectedDungeon(kHeroic, 0u);
  CHECK(lfg.GetSelectedDungeons() == std::vector<std::uint32_t>{kDungeon, kHeroic});

  // Adding again doesn't duplicate.
  lfg.AddSelectedDungeon(kHeroic, 0u);
  CHECK(lfg.GetSelectedDungeons().size() == 2u);

  // A random dungeon replaces the whole selection.
  lfg.AddSelectedDungeon(kRandom, 0u);
  CHECK(lfg.GetSelectedDungeons() == std::vector<std::uint32_t>{kRandom});

  lfg.RemoveSelectedDungeon(kRandom);
  CHECK(lfg.GetSelectedDungeons().empty());
  lfg.RemoveSelectedDungeon(kRandom);  // absent: no-op

  // Raids stack only while solo.
  lfg.AddSelectedDungeon(kRaid, 0u);
  lfg.AddSelectedDungeon(kRaid + 1u, 0u);
  CHECK(lfg.GetSelectedDungeons().size() == 2u);
  lfg.AddSelectedDungeon(kRaid + 2u, 2u);
  CHECK(lfg.GetSelectedDungeons() == std::vector<std::uint32_t>{kRaid + 2u});
  CHECK(lfg.ContainsSelectedDungeon(kRaid + 2u));
  CHECK_FALSE(lfg.ContainsSelectedDungeon(kRaid));
}

TEST_CASE("LFGSystem join requests: one in flight, re-sent when the selection changes",
          "[lfg][system]") {
  auto& lfg = Fresh();
  const auto t0 = LFGSystem::JoinThrottleClock::time_point{} + std::chrono::hours(1);

  CHECK(lfg.TryBeginJoinRequest(t0));
  lfg.JoinQueue({kDungeon}, 0x08u);
  CHECK(lfg.IsQueued());
  CHECK(lfg.HasActiveJoinRequest());
  // Same selection while queued: nothing to send.
  CHECK_FALSE(lfg.TryBeginJoinRequest(t0));

  // Changing the selection allows a re-send.
  lfg.AddSelectedDungeon(kHeroic, 0u);
  CHECK(lfg.TryBeginJoinRequest(t0));

  lfg.LeaveQueue();
  CHECK_FALSE(lfg.HasActiveJoinRequest());
  CHECK(lfg.GetState() == LFGState::None);
}

TEST_CASE("LFGSystem join throttle: two free sends, then one per 10 s window",
          "[lfg][system]") {
  auto& lfg = Fresh();
  const auto t0 = LFGSystem::JoinThrottleClock::time_point{} + std::chrono::hours(1);
  CHECK(lfg.TryBeginJoinRequest(t0));
  CHECK(lfg.TryBeginJoinRequest(t0));
  // The third attempt opens a window (the anchor was unset).
  CHECK(lfg.TryBeginJoinRequest(t0));
  CHECK(lfg.TryBeginJoinRequest(t0 + std::chrono::seconds(1)));
  CHECK(lfg.TryBeginJoinRequest(t0 + std::chrono::seconds(2)));
  CHECK_FALSE(lfg.TryBeginJoinRequest(t0 + std::chrono::seconds(3)));
  CHECK_FALSE(lfg.TryBeginJoinRequest(t0 + std::chrono::seconds(9)));
  CHECK(lfg.TryBeginJoinRequest(t0 + std::chrono::seconds(10)));
}

TEST_CASE("LFGSystem comment throttle: two free sends, then 2 s windows",
          "[lfg][system]") {
  auto& lfg = Fresh();
  const auto t0 = LFGSystem::CommentThrottleClock::time_point{} + std::chrono::hours(1);
  CHECK(lfg.TryConsumeCommentSendThrottleToken(t0));
  CHECK(lfg.TryConsumeCommentSendThrottleToken(t0));
  CHECK(lfg.TryConsumeCommentSendThrottleToken(t0));  // opens the window
  CHECK(lfg.TryConsumeCommentSendThrottleToken(t0));
  CHECK(lfg.TryConsumeCommentSendThrottleToken(t0));
  CHECK_FALSE(lfg.TryConsumeCommentSendThrottleToken(t0 + std::chrono::milliseconds(1999)));
  CHECK(lfg.TryConsumeCommentSendThrottleToken(t0 + std::chrono::seconds(2)));
}

TEST_CASE("LFGSystem teleport throttle: two free sends, then 10 s windows",
          "[lfg][system]") {
  auto& lfg = Fresh();
  CHECK(lfg.TryPrepareTeleportSend(100.0));
  CHECK(lfg.TryPrepareTeleportSend(100.0));
  CHECK(lfg.TryPrepareTeleportSend(100.0));  // window anchor was 0 s
  CHECK(lfg.TryPrepareTeleportSend(101.0));
  CHECK(lfg.TryPrepareTeleportSend(102.0));
  CHECK_FALSE(lfg.TryPrepareTeleportSend(109.9));
  CHECK(lfg.TryPrepareTeleportSend(110.0));
}

TEST_CASE("LFGSystem boot votes", "[lfg][system]") {
  auto& lfg = Fresh();
  const auto t0 = LFGSystem::CommentThrottleClock::time_point{} + std::chrono::hours(1);

  // No vote in progress: nothing to send.
  CHECK_FALSE(lfg.TryPrepareBootVoteSend(true, t0));
  CHECK(lfg.GetBootVotesNeeded() == 0u);

  lfg.SetBootVoteInProgress(true);
  CHECK(lfg.IsBootVoteInProgress());
  CHECK(lfg.GetBootVotesNeeded() == 3u);
  CHECK(lfg.TryPrepareBootVoteSend(false, t0));
  CHECK(lfg.GetBootVoteSnapshot()->my_vote == false);

  lfg.SetBootVoteCount(2u, 1u);
  CHECK(lfg.GetBootVoteYes() == 2u);
  CHECK(lfg.GetBootVoteNo() == 1u);

  lfg.VoteToBoot(true);
  CHECK(lfg.GetBootVoteYes() == 3u);
  CHECK(lfg.GetBootVoteNo() == 1u);
  CHECK(lfg.GetBootVoteSnapshot()->did_vote);

  lfg.SetBootTarget(openwow::game::ObjectGuid(0x42u));
  CHECK(lfg.GetBootTarget().GetRawValue() == 0x42u);
}

TEST_CASE("LFGSystem proposals", "[lfg][system]") {
  auto& lfg = Fresh();
  LFGProposal proposal;
  proposal.id = 7u;
  proposal.members.push_back({.guid = 1u, .self = true});
  proposal.members.push_back({.guid = 2u});
  lfg.SetProposal(proposal);

  lfg.ApplyProposalResponse(true);
  REQUIRE(lfg.HasProposal());
  CHECK(lfg.GetProposal()->members[0].accepted == true);
  CHECK_FALSE(lfg.GetProposal()->members[1].accepted.has_value());

  lfg.ApplyProposalResponse(false);
  CHECK_FALSE(lfg.HasProposal());
}

TEST_CASE("LFGSystem queue info enters the queue once", "[lfg][system]") {
  auto& lfg = Fresh();
  lfg.SetQueueInfo(kDungeon, 30u, 45u, 12u, false);
  CHECK(lfg.GetState() == LFGState::None);
  CHECK(lfg.GetAverageWaitTime() == 30u);
  lfg.SetQueueInfo(kDungeon, 30u, 45u, 12u, true);
  CHECK(lfg.GetState() == LFGState::Queued);
  lfg.SetState(LFGState::Proposal);
  lfg.SetQueueInfo(kDungeon, 30u, 45u, 13u, true);
  CHECK(lfg.GetState() == LFGState::Proposal);
  CHECK(lfg.GetTimeInQueue() == 13u);
}
