#include <ArduinoJson.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include "PrioritiesStore.h"
#include "util/TaskSync.h"

namespace {
JsonDocument parse(const std::string& json) {
  JsonDocument doc;
  EXPECT_FALSE(deserializeJson(doc, json)) << json;
  return doc;
}
std::string roundTrip(const std::vector<std::string>& frames) {
  tasksync::Reassembler reassembler;
  tasksync::Reassembler::Result result = tasksync::Reassembler::Result::NeedMore;
  for (const auto& frame : frames)
    result = reassembler.push(reinterpret_cast<const uint8_t*>(frame.data()), frame.size());
  EXPECT_EQ(result, tasksync::Reassembler::Result::Complete);
  return reassembler.message();
}
}  // namespace

TEST(TaskSyncFraming, SingleFrameHasFirstAndLast) {
  const auto frames = tasksync::toFrames("hello", 20);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(static_cast<uint8_t>(frames[0][0]), tasksync::FRAME_FIRST | tasksync::FRAME_LAST);
  EXPECT_EQ(roundTrip(frames), "hello");
}

TEST(TaskSyncFraming, LongMessageSplitsAndRebuilds) {
  std::string message;
  for (int i = 0; i < 1000; ++i) message += static_cast<char>('a' + i % 26);
  const auto frames = tasksync::toFrames(message, 20);
  EXPECT_EQ(frames.size(), (1000u + 18u) / 19u);
  for (const auto& frame : frames) EXPECT_LE(frame.size(), 20u);
  EXPECT_EQ(roundTrip(frames), message);
}

TEST(TaskSyncFraming, EmptyMessageStillSendsOneFrame) {
  const auto frames = tasksync::toFrames("", 20);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(roundTrip(frames), "");
}

TEST(TaskSyncFraming, ContinuationWithoutStartIsRejected) {
  tasksync::Reassembler reassembler;
  const uint8_t orphan[] = {tasksync::FRAME_LAST, 'x'};
  EXPECT_EQ(reassembler.push(orphan, sizeof(orphan)), tasksync::Reassembler::Result::Error);
}

TEST(TaskSyncFraming, NewFirstFrameDropsHalfFinishedMessage) {
  tasksync::Reassembler reassembler;
  const uint8_t half[] = {tasksync::FRAME_FIRST, 'o', 'l', 'd'};
  const uint8_t fresh[] = {tasksync::FRAME_FIRST | tasksync::FRAME_LAST, 'n', 'e', 'w'};
  EXPECT_EQ(reassembler.push(half, sizeof(half)), tasksync::Reassembler::Result::NeedMore);
  EXPECT_EQ(reassembler.push(fresh, sizeof(fresh)), tasksync::Reassembler::Result::Complete);
  EXPECT_EQ(reassembler.message(), "new");
}

TEST(TaskSyncFraming, OversizedMessageIsDropped) {
  tasksync::Reassembler reassembler;
  std::string chunk(1, static_cast<char>(tasksync::FRAME_FIRST));
  chunk.append(tasksync::MAX_MESSAGE, 'x');
  EXPECT_EQ(reassembler.push(reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size()),
            tasksync::Reassembler::Result::NeedMore);
  const uint8_t more[] = {tasksync::FRAME_LAST, 'y'};
  EXPECT_EQ(reassembler.push(more, sizeof(more)), tasksync::Reassembler::Result::Error);
}

TEST(TaskSyncAuth, EverythingIsRefusedUntilTheCodeMatches) {
  PrioritiesStore store;
  ASSERT_TRUE(store.parse(R"({"items":[{"id":"t1","title":"Secret"}]})"));
  tasksync::Session session{"482916"};
  for (const char* request : {R"({"cmd":"list"})", R"({"cmd":"add","title":"x"})", "garbage"}) {
    const auto doc = parse(tasksync::handleRequest(store, session, request));
    EXPECT_FALSE(doc["ok"].as<bool>());
    EXPECT_STREQ(doc["err"], "unauthorized");
    EXPECT_EQ(doc["items"].size(), 0u);  // the list is not leaked
  }
  EXPECT_EQ(store.items().size(), 1u);  // and nothing was edited
}

TEST(TaskSyncAuth, RightCodeUnlocksTheSession) {
  PrioritiesStore store;
  ASSERT_TRUE(store.parse(R"({"items":[{"id":"t1","title":"One"}]})"));
  tasksync::Session session{"482916"};
  const std::string auth = R"({"cmd":"auth","code":"482916"})";
  auto doc = parse(tasksync::handleRequest(store, session, auth));
  EXPECT_TRUE(doc["ok"].as<bool>());
  EXPECT_EQ(doc["items"].size(), 1u);  // the auth reply doubles as the first snapshot
  EXPECT_TRUE(session.authenticated);
  doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"list"})"));
  EXPECT_TRUE(doc["ok"].as<bool>());
}

TEST(TaskSyncAuth, WrongCodesAreCountedAndEventuallyDropTheCaller) {
  PrioritiesStore store;
  tasksync::Session session{"482916"};
  for (int i = 0; i < tasksync::MAX_BAD_ATTEMPTS; ++i) {
    EXPECT_FALSE(session.shouldDrop());
    const auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"auth","code":"000000"})"));
    EXPECT_STREQ(doc["err"], "bad_code");
  }
  EXPECT_TRUE(session.shouldDrop());
  EXPECT_FALSE(session.authenticated);
  // Even the right code is refused once the limit is reached.
  const std::string auth = R"({"cmd":"auth","code":"482916"})";
  EXPECT_STREQ(parse(tasksync::handleRequest(store, session, auth))["err"], "bad_code");
  EXPECT_FALSE(session.authenticated);
}

TEST(TaskSyncAuth, ASecondAuthOnAnOpenSessionIsHarmless) {
  PrioritiesStore store;
  tasksync::Session session{"482916", true, 0};
  EXPECT_TRUE(parse(tasksync::handleRequest(store, session, R"({"cmd":"auth","code":"nope"})"))["ok"].as<bool>());
}

TEST(TaskSyncAuth, ASessionWithoutACodeRefusesEveryAttempt) {
  PrioritiesStore store;
  tasksync::Session session;  // code file unreadable
  EXPECT_STREQ(parse(tasksync::handleRequest(store, session, R"({"cmd":"auth","code":""})"))["err"], "bad_code");
  EXPECT_FALSE(session.authenticated);
}

TEST(TaskSyncCode, FileContentIsTrimmedAndValidated) {
  EXPECT_EQ(tasksync::normalizeCode("482916\n"), "482916");
  EXPECT_EQ(tasksync::normalizeCode("  abc123 \r\n"), "abc123");
  EXPECT_EQ(tasksync::normalizeCode(""), "");
  EXPECT_EQ(tasksync::normalizeCode("   \n"), "");
  EXPECT_EQ(tasksync::normalizeCode("has space"), "");
  EXPECT_EQ(tasksync::normalizeCode("quote\"d"), "");
  EXPECT_EQ(tasksync::normalizeCode(std::string(32, 'a')), std::string(32, 'a'));
  EXPECT_EQ(tasksync::normalizeCode(std::string(33, 'a')), "");
}

TEST(TaskSyncCode, GeneratedCodesAreSixDigitsWithLeadingZeros) {
  EXPECT_EQ(tasksync::codeFromRandom(7), "000007");
  EXPECT_EQ(tasksync::codeFromRandom(1234567), "234567");
  EXPECT_EQ(tasksync::normalizeCode(tasksync::codeFromRandom(99)), "000099");
}

TEST(TaskSyncIdle, PausesOnlyWhenNobodyIsConnectedForTheWholeLimit) {
  constexpr uint32_t limit = tasksync::IDLE_PAUSE_MS;
  EXPECT_FALSE(tasksync::shouldPauseIdle(false, limit - 1));
  EXPECT_TRUE(tasksync::shouldPauseIdle(false, limit));
  EXPECT_FALSE(tasksync::shouldPauseIdle(true, limit * 10));  // a connected phone is never idle
  EXPECT_EQ(limit, 300000u);
}

class TaskSyncCommands : public ::testing::Test {
 protected:
  void SetUp() override {
    Storage.failWrites = false;
    ASSERT_TRUE(store.parse(R"({"items":[{"id":"t1","title":"One"},{"id":"t2","title":"Two","done":true}]})"));
  }
  PrioritiesStore store;
  tasksync::Session session{"482916", true, 0};  // most tests run after a successful auth
};

TEST_F(TaskSyncCommands, ListReturnsSnapshotAndCapacity) {
  const auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"list"})"));
  EXPECT_TRUE(doc["ok"].as<bool>());
  EXPECT_EQ(doc["v"].as<int>(), 1);
  EXPECT_EQ(doc["cap"].as<int>(), 10);
  EXPECT_EQ(doc["items"].size(), 2u);
  EXPECT_EQ(doc["items"][1]["done"].as<bool>(), true);
}

TEST_F(TaskSyncCommands, AddAssignsNextIdAndPersists) {
  const auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"add","title":"Three","note":"n"})"));
  EXPECT_TRUE(doc["ok"].as<bool>());
  ASSERT_EQ(doc["items"].size(), 3u);
  EXPECT_STREQ(doc["items"][2]["id"], "t3");
  EXPECT_STREQ(doc["items"][2]["note"], "n");
  PrioritiesStore reloaded;
  ASSERT_TRUE(reloaded.parse(Storage.file));
  EXPECT_EQ(reloaded.items().size(), 3u);
}

TEST_F(TaskSyncCommands, AddRejectsBlankTitleAndFullList) {
  auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"add","title":""})"));
  EXPECT_FALSE(doc["ok"].as<bool>());
  EXPECT_STREQ(doc["err"], "bad_request");
  for (int i = 0; i < 8; ++i) tasksync::handleRequest(store, session, R"({"cmd":"add","title":"x"})");
  doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"add","title":"overflow"})"));
  EXPECT_STREQ(doc["err"], "full");
  EXPECT_EQ(doc["items"].size(), 10u);  // snapshot still comes back
}

TEST_F(TaskSyncCommands, ToggleAndDeleteById) {
  auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"toggle","id":"t1","done":true})"));
  EXPECT_TRUE(doc["ok"].as<bool>());
  EXPECT_TRUE(doc["items"][0]["done"].as<bool>());
  doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"delete","id":"t1"})"));
  EXPECT_TRUE(doc["ok"].as<bool>());
  ASSERT_EQ(doc["items"].size(), 1u);
  EXPECT_STREQ(doc["items"][0]["id"], "t2");
}

TEST_F(TaskSyncCommands, UnknownIdAndBadRequests) {
  EXPECT_STREQ(parse(tasksync::handleRequest(store, session, R"({"cmd":"delete","id":"zz"})"))["err"], "not_found");
  EXPECT_STREQ(parse(tasksync::handleRequest(store, session, R"({"cmd":"toggle","id":"t1"})"))["err"], "bad_request");
  EXPECT_STREQ(parse(tasksync::handleRequest(store, session, R"({"cmd":"nope"})"))["err"], "bad_request");
  EXPECT_STREQ(parse(tasksync::handleRequest(store, session, "not json"))["err"], "bad_request");
}

TEST_F(TaskSyncCommands, SetReplacesTheListInOrderAndKeepsIds) {
  const auto doc = parse(tasksync::handleRequest(
      store, session, R"({"cmd":"set","items":[{"id":"t2","title":"Two","done":false},{"id":"t1","title":"One","done":true}]})"));
  EXPECT_TRUE(doc["ok"].as<bool>());
  ASSERT_EQ(doc["items"].size(), 2u);
  EXPECT_STREQ(doc["items"][0]["id"], "t2");
  EXPECT_FALSE(doc["items"][0]["done"].as<bool>());
  EXPECT_STREQ(doc["items"][1]["id"], "t1");
  PrioritiesStore reloaded;
  ASSERT_TRUE(reloaded.parse(Storage.file));
  EXPECT_EQ(reloaded.items()[0].id, "t2");
}

TEST_F(TaskSyncCommands, SetAssignsIdsToNewAndDuplicateItems) {
  const auto doc = parse(tasksync::handleRequest(
      store, session, R"({"cmd":"set","items":[{"id":"t5","title":"A"},{"title":"New"},{"id":"t5","title":"Dup"}]})"));
  ASSERT_TRUE(doc["ok"].as<bool>());
  EXPECT_STREQ(doc["items"][0]["id"], "t5");
  EXPECT_STREQ(doc["items"][1]["id"], "t6");
  EXPECT_STREQ(doc["items"][2]["id"], "t7");
}

TEST_F(TaskSyncCommands, SetCanEmptyTheList) {
  const auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"set","items":[]})"));
  EXPECT_TRUE(doc["ok"].as<bool>());
  EXPECT_EQ(doc["items"].size(), 0u);
}

TEST_F(TaskSyncCommands, SetIsAllOrNothing) {
  auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"set","items":[{"title":"ok"},{"title":""}]})"));
  EXPECT_STREQ(doc["err"], "bad_request");
  EXPECT_EQ(doc["items"].size(), 2u);  // unchanged
  std::string many = R"({"cmd":"set","items":[)";
  for (int i = 0; i < 11; ++i) many += std::string(i ? "," : "") + R"({"title":"x"})";
  many += "]}";
  doc = parse(tasksync::handleRequest(store, session, many));
  EXPECT_STREQ(doc["err"], "full");
  EXPECT_EQ(doc["items"].size(), 2u);
  EXPECT_STREQ(parse(tasksync::handleRequest(store, session, R"({"cmd":"set"})"))["err"], "bad_request");
}

TEST_F(TaskSyncCommands, SetRollsBackOnFailedWrite) {
  Storage.failWrites = true;
  const auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"set","items":[{"title":"lost"}]})"));
  EXPECT_STREQ(doc["err"], "save_failed");
  EXPECT_EQ(doc["items"].size(), 2u);
  EXPECT_STREQ(doc["items"][0]["title"], "One");
}

TEST_F(TaskSyncCommands, FailedWriteRollsBackAndReports) {
  Storage.failWrites = true;
  auto doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"add","title":"lost"})"));
  EXPECT_STREQ(doc["err"], "save_failed");
  EXPECT_EQ(doc["items"].size(), 2u);
  doc = parse(tasksync::handleRequest(store, session, R"({"cmd":"delete","id":"t1"})"));
  EXPECT_EQ(doc["items"].size(), 2u);
}
