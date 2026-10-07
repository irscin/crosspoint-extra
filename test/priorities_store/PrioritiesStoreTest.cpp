#include <HalStorage.h>
#include <gtest/gtest.h>

#include "PrioritiesStore.h"

TEST(PrioritiesStore, ParsesItemsAndTallies) {
  PrioritiesStore store;
  ASSERT_TRUE(store.parse(R"({"version":1,"items":[
    {"id":"a","title":"Write report","note":"by noon","done":false},
    {"id":"b","title":"Call mum","done":true}]})"));
  ASSERT_EQ(store.items().size(), 2u);
  EXPECT_EQ(store.items()[0].title, "Write report");
  EXPECT_EQ(store.items()[0].note, "by noon");
  EXPECT_TRUE(store.items()[1].done);
  int active = 0, done = 0;
  store.tally(active, done);
  EXPECT_EQ(active, 1);
  EXPECT_EQ(done, 1);
}

TEST(PrioritiesStore, RejectsInvalidAndKeepsPreviousList) {
  PrioritiesStore store;
  ASSERT_TRUE(store.parse(R"({"items":[{"id":"a","title":"One"}]})"));
  EXPECT_FALSE(store.parse("not json"));
  EXPECT_FALSE(store.parse(R"({"items":5})"));
  EXPECT_EQ(store.items().size(), 1u);
}

TEST(PrioritiesStore, SkipsUntitledAndCapsAtTen) {
  std::string json = R"({"items":[{"id":"x","title":""})";
  for (int i = 0; i < 14; ++i) json += R"(,{"id":"i","title":"T"})";
  json += "]}";
  PrioritiesStore store;
  ASSERT_TRUE(store.parse(json));
  EXPECT_EQ(store.items().size(), PrioritiesStore::CAPACITY);
}

TEST(PrioritiesStore, ClipsLongTitleOnUtf8Boundary) {
  std::string title;
  for (int i = 0; i < 60; ++i) title += "é";  // 120 bytes
  PrioritiesStore store;
  ASSERT_TRUE(store.parse(R"({"items":[{"id":"a","title":")" + title + R"("}]})"));
  const std::string& clipped = store.items()[0].title;
  EXPECT_LE(clipped.size(), PrioritiesStore::MAX_TITLE);
  EXPECT_EQ(clipped.size(), PrioritiesStore::MAX_TITLE);  // 96 = 48 whole characters
}

TEST(PrioritiesStore, SetDonePersistsAndRoundTrips) {
  PrioritiesStore store;
  ASSERT_TRUE(store.parse(R"({"items":[{"id":"a","title":"One"}]})"));
  ASSERT_TRUE(store.setDone(0, true));
  PrioritiesStore reloaded;
  ASSERT_TRUE(reloaded.parse(Storage.file));
  EXPECT_TRUE(reloaded.items()[0].done);
}

TEST(PrioritiesStore, SetDoneRevertsWhenWriteFails) {
  PrioritiesStore store;
  ASSERT_TRUE(store.parse(R"({"items":[{"id":"a","title":"One"}]})"));
  Storage.failWrites = true;
  EXPECT_FALSE(store.setDone(0, true));
  Storage.failWrites = false;
  EXPECT_FALSE(store.items()[0].done);
  EXPECT_FALSE(store.setDone(5, true));
}
