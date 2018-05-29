#include "src/AbstractDomain/common/dimension.hpp"
#include "gtest/gtest.h"

using namespace abstract_domain;

DimensionKey dummyKey(std::string("dummy"), 0, utils::thirty_two);
TEST(DimensionTest, isVersionInDimension) {
  EXPECT_TRUE (isVersionInDimension(dummyKey, Version(0)));
  EXPECT_FALSE(isVersionInDimension(dummyKey, Version(1)));
} 

TEST(DimensionTest, replaceVersion) {
  DimensionKey dummyKey_01 = replaceVersion(dummyKey, 0, 1);
  DimensionKey dummyKey_withver1(std::string("dummy"), 1, utils::thirty_two);
  EXPECT_TRUE (dummyKey_01 == dummyKey_withver1);
  EXPECT_FALSE(dummyKey_01 == dummyKey);
  DimensionKey dummyKey_withver1_wrongsize(std::string("dummy"), 1, utils::sixteen);
  EXPECT_FALSE(dummyKey_01 == dummyKey_withver1_wrongsize);
} 

TEST(DimensionTest, replaceVersions) {
  std::map<Version, Version> map_0_1;
  map_0_1.insert(std::pair<Version,Version>(0,1));
  std::map<Version, Version> map_1_0;
  map_1_0.insert(std::pair<Version,Version>(1,0));
  DimensionKey dummyKey_01 = replaceVersions(dummyKey, map_0_1);
  DimensionKey dummyKey_withver1(std::string("dummy"), 1, utils::thirty_two);
  EXPECT_TRUE (dummyKey_01 == dummyKey_withver1);
  EXPECT_FALSE(dummyKey_01 == dummyKey);
  DimensionKey dummyKey_withver1_wrongsize(std::string("dummy"), 1, utils::sixteen);
  EXPECT_FALSE(dummyKey_01 == dummyKey_withver1_wrongsize);

  DimensionKey dummyKey_10 = replaceVersions(dummyKey, map_1_0);
  EXPECT_FALSE(dummyKey_10 == dummyKey_withver1);
  EXPECT_TRUE (dummyKey_10 == dummyKey);
} 

TEST(DimensionTest, getVocabularySubset) {
  DimensionKey dummyKey2(std::string("dummy2"), 1, utils::thirty_two);
  Vocabulary v;
  v.insert(dummyKey);
  v.insert(dummyKey2);
  Vocabulary v_0 = getVocabularySubset(v, 0);
  EXPECT_EQ(1u, v_0.size());
  EXPECT_TRUE(dummyKey == *(v_0.begin()));
  Vocabulary v_1 = getVocabularySubset(v, 1);
  EXPECT_EQ(1u, v_1.size());
  EXPECT_TRUE(dummyKey2 == *(v_1.begin()));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
