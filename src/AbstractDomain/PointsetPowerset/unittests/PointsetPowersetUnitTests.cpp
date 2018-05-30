#include "src/AbstractDomain/PointsetPowerset/pointset_powerset_av.hpp"

//Avoid mutiple macro redefinition
#define GTEST_DONT_DEFINE_TEST 1
#include "gtest/gtest.h"

using namespace abstract_domain;
using namespace Parma_Polyhedra_Library;

typedef AbstractValue AV;
typedef PointsetPowersetAv<C_Polyhedron> PP_CPOLY_AV;
typedef PointsetPowersetAv<Octagonal_Shape<mpz_class> > PP_OCT_AV;

class AvTestInfo {
public:
  virtual void Initialize() {
  }
  virtual void Finalize() {}
  AvTestInfo() {}
  virtual ~AvTestInfo() {}

  wali::ref_ptr<AbstractValue> av;
  DimensionKey k0, k1, k2, k3, k4 ,k5, k6, k7;
};

class AvTest : public ::testing::TestWithParam<std::shared_ptr<AvTestInfo> > {
public:
  AvTest() {
    avtestinfo_ = GetParam();
    avtestinfo_->Initialize();
  }  

  ~AvTest() {
    avtestinfo_->Finalize();
  }  
  
  virtual void SetUp() {
  }
 
 wali::ref_ptr<AV> ma() {
   wali::ref_ptr<AV> ret = avtestinfo_->av->Copy();
   ret->AddEquality(avtestinfo_->k0, avtestinfo_->k1);
   ret->AddEquality(avtestinfo_->k3, avtestinfo_->k2);
   return ret;
 }

  wali::ref_ptr<AV> mb() {
    wali::ref_ptr<AV> ret = avtestinfo_->av->Copy();
    ret->AddEquality(avtestinfo_->k0, avtestinfo_->k5);
    return ret;
  }

  wali::ref_ptr<AV> mc() {
    wali::ref_ptr<AV> ret = avtestinfo_->av->Copy();
    ret->AddEquality(avtestinfo_->k1, avtestinfo_->k2);
    ret->AddEquality(avtestinfo_->k3, avtestinfo_->k4);
    return ret;
  }

  wali::ref_ptr<AV> md() {
    wali::ref_ptr<AV> ret = avtestinfo_->av->Copy();
    ret->AddEquality(avtestinfo_->k0, avtestinfo_->k5);
    return ret;
  }

  static std::shared_ptr<AvTestInfo> avtestinfo_;

};

std::shared_ptr<AvTestInfo> AvTest::avtestinfo_;

//Constructor
TEST_P(AvTest, CopyConstructor) {
  Vocabulary v;
  v.insert(avtestinfo_->k0);
  v.insert(avtestinfo_->k1);
  wali::ref_ptr<AbstractValue> c = avtestinfo_->av->Copy();
  EXPECT_EQ(*c, *avtestinfo_->av);

  c->AddEquality(avtestinfo_->k0, avtestinfo_->k1);

  wali::ref_ptr<AbstractValue> copy = c->Copy();
  EXPECT_EQ(*c, *copy);
}

TEST_P(AvTest, TopIsTop) {
  EXPECT_FALSE(avtestinfo_->av->Top()->IsBottom());
  EXPECT_TRUE(avtestinfo_->av->Top()->IsTop());
}


TEST_P(AvTest, BottomIsbottom) {
  EXPECT_FALSE(avtestinfo_->av->Bottom()->IsTop());
  EXPECT_TRUE(avtestinfo_->av->Bottom()->IsBottom());
}

TEST_P(AvTest, TopJoinBot) {
  wali::ref_ptr<AV> topJoinBot = avtestinfo_->av->Top();
  topJoinBot->Join(avtestinfo_->av->Bottom());

  wali::ref_ptr<AV> tp = avtestinfo_->av->Top();
 
  EXPECT_EQ(*tp, *topJoinBot);
}

TEST_P(AvTest, BotJoinTop) {
  wali::ref_ptr<AV> botJoinTop = avtestinfo_->av->Bottom();
  botJoinTop->Join(avtestinfo_->av->Top());

  wali::ref_ptr<AV> tp = avtestinfo_->av->Top();
 
  EXPECT_EQ(*tp, *botJoinTop);
}

TEST_P(AvTest, BotMeetTop) {
  wali::ref_ptr<AV> botMeetTop = avtestinfo_->av->Bottom();
  botMeetTop->Meet(avtestinfo_->av->Top());

  wali::ref_ptr<AV> bt = avtestinfo_->av->Bottom();
 
  EXPECT_EQ(*bt, *botMeetTop);
}

TEST_P(AvTest, TopMeetBot) {
  wali::ref_ptr<AV> topMeetBot = avtestinfo_->av->Top();
  topMeetBot->Meet(avtestinfo_->av->Bottom());

  wali::ref_ptr<AV> bt = avtestinfo_->av->Bottom();

  EXPECT_EQ(*bt, *topMeetBot);
}

TEST_P(AvTest, CommuteJoin) {
  wali::ref_ptr<AV> aJoinB = ma()->Copy();
  aJoinB->Join(mb());

  wali::ref_ptr<AV> bJoinA = mb()->Copy();
  bJoinA->Join(ma());

  EXPECT_EQ(*aJoinB, *bJoinA);
}

TEST_P(AvTest, CommuteMeet) {
  wali::ref_ptr<AV> aMeetB = ma()->Copy();
  aMeetB->Meet(mb());

  wali::ref_ptr<AV> bMeetA = mb()->Copy();
  bMeetA->Meet(ma());

  EXPECT_EQ(*aMeetB, *bMeetA);
}

TEST_P(AvTest, CommuteMeet2) {
  wali::ref_ptr<AV> cMeetD = mc()->Copy();
  cMeetD->Meet(md());

  wali::ref_ptr<AV> dMeetC = md()->Copy();
  dMeetC->Meet(mc());

  EXPECT_EQ(*cMeetD, *dMeetC);
}

TEST_P(AvTest, AddColsToTop) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Copy();
  Vocabulary new_voc;
  new_voc.insert(avtestinfo_->k6);
  a->AddVocabulary(new_voc);
  EXPECT_EQ(7u, a->NumVars());
  EXPECT_TRUE(a->IsTop());

  Vocabulary exis_voc;
  exis_voc.insert(avtestinfo_->k1);
  a->AddVocabulary(exis_voc);
  EXPECT_EQ(7u, a->NumVars()); //No increase in num vars as the vocabulary already existed in a.
}

TEST_P(AvTest, AddColsToBot) {
  wali::ref_ptr<AV> bot = avtestinfo_->av->Bottom();
  Vocabulary new_voc;
  new_voc.insert(avtestinfo_->k6);
  new_voc.insert(avtestinfo_->k7);
  bot->AddVocabulary(new_voc);
  EXPECT_EQ(8u, bot->NumVars());
  EXPECT_TRUE(bot->IsBottom());
}

TEST_P(AvTest, BotSubsetTop) {
  wali::ref_ptr<AV> t = avtestinfo_->av->Top();
  wali::ref_ptr<AV> b = avtestinfo_->av->Bottom();
  EXPECT_TRUE(t->Overapproximates(b));
  EXPECT_FALSE(b->Overapproximates(t));

  EXPECT_TRUE(t->Overapproximates(t));
  EXPECT_TRUE(b->Overapproximates(b));
}

TEST_P(AvTest, MiddleSubsets) {
  wali::ref_ptr<AV> a = ma();

  EXPECT_TRUE(a->Overapproximates(a->Bottom()));
  EXPECT_TRUE(a->Top()->Overapproximates(a));
    
  EXPECT_FALSE(a->Bottom()->Overapproximates(a));
}

TEST_P(AvTest, AddEquality) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  a->AddEquality(avtestinfo_->k0, avtestinfo_->k0); // This equality should do nothing
  EXPECT_TRUE(a->IsTop());

  a->AddEquality(avtestinfo_->k0, avtestinfo_->k1);

  wali::ref_ptr<AV> b = avtestinfo_->av->Top();
  b->AddEquality(avtestinfo_->k1, avtestinfo_->k2);

  a->AddEquality(avtestinfo_->k1, avtestinfo_->k2);
  EXPECT_TRUE(b->Overapproximates(a));

  b->AddEquality(avtestinfo_->k0, avtestinfo_->k1);
  EXPECT_EQ(*a, *b);

  a->AddEquality(avtestinfo_->k0, avtestinfo_->k2);
  EXPECT_EQ(*a, *b);
}

TEST_P(AvTest, Project) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  a->AddEquality(avtestinfo_->k0, avtestinfo_->k2);

  Vocabulary v;
  v.insert(avtestinfo_->k0);
  v.insert(avtestinfo_->k1);
  v.insert(avtestinfo_->k2);
  v.insert(avtestinfo_->k3);

  wali::ref_ptr<AV> b = a->Copy(); 
  b->Project(v);
  EXPECT_EQ(4u, b->NumVars());

  Vocabulary v_3var;
  v_3var.insert(avtestinfo_->k1);
  v_3var.insert(avtestinfo_->k2);
  v_3var.insert(avtestinfo_->k3);

  wali::ref_ptr<AV> c = a->Copy();
  c->Project(v_3var);
  EXPECT_EQ(3u, c->NumVars());
  EXPECT_TRUE(c->IsTop());

  Vocabulary v_3var_2;
  v_3var_2.insert(avtestinfo_->k0);
  v_3var_2.insert(avtestinfo_->k1);
  v_3var_2.insert(avtestinfo_->k2);

  wali::ref_ptr<AV> d = a->Copy();
  d->Project(v_3var_2);
  EXPECT_EQ(3u, d->NumVars());
}

TEST_P(AvTest, HavocBottom) {
  wali::ref_ptr<AV> bot = avtestinfo_->av->Bottom();

  Vocabulary v;
  v.insert(avtestinfo_->k4);
  v.insert(avtestinfo_->k5);

  bot->Havoc(v);
  EXPECT_TRUE(bot->IsBottom());

  bot->Havoc(bot->GetVocabulary());
  EXPECT_TRUE(bot->IsBottom());
}

TEST_P(AvTest, Havoc) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  a->AddEquality(avtestinfo_->k0, avtestinfo_->k2);

  Vocabulary v;
  v.insert(avtestinfo_->k4);
  v.insert(avtestinfo_->k5);

  wali::ref_ptr<AV> b = a->Copy();
  b->Havoc(v);
  EXPECT_EQ(6u, b->NumVars());

  Vocabulary v_k0;
  v_k0.insert(avtestinfo_->k0);

  wali::ref_ptr<AV> c = a->Havoc(v_k0);
  EXPECT_TRUE(c->IsTop());
}

TEST_P(AvTest, AddDimension) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  a->AddEquality(avtestinfo_->k0, avtestinfo_->k2);
  a->AddDimension(avtestinfo_->k5);
  EXPECT_EQ(6u, a->NumVars());
}

TEST_P(AvTest, AddVocabulary) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  a->AddEquality(avtestinfo_->k0, avtestinfo_->k2);
  wali::ref_ptr<AV> b = a->Copy();
  a->AddDimension(avtestinfo_->k5);
  a->AddDimension(avtestinfo_->k6);
  EXPECT_EQ(7u, a->NumVars());

  Vocabulary v;
  v.insert(avtestinfo_->k5);
  v.insert(avtestinfo_->k6);

  b->AddVocabulary(v);
  EXPECT_EQ(7u, b->NumVars());
  EXPECT_EQ(*a, *b);
}

TEST_P(AvTest, AddVocabularyBottom) {
  Vocabulary v;
  v.insert(avtestinfo_->k0);
  v.insert(avtestinfo_->k1);

  wali::ref_ptr<AV> a = avtestinfo_->av->Copy();
  a->RemoveVocabulary(v);
  
  wali::ref_ptr<AV> bot = a->Bottom();

  wali::ref_ptr<AV> b = bot->Copy();  
  b->AddDimension(avtestinfo_->k0);
  EXPECT_TRUE(b->IsBottom());


  wali::ref_ptr<AV> c = bot->Copy();
  c->AddVocabulary(v);
  EXPECT_TRUE(c->IsBottom());
}

TEST_P(AvTest, ReplaceVersion) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  a->AddEquality(avtestinfo_->k2, avtestinfo_->k5);

  wali::ref_ptr<AV> b = a->Copy();
  b->ReplaceVersion(Version(0), Version(0));
  EXPECT_EQ(*a, *b);

  b->ReplaceVersion(Version(1), Version(2));
  EXPECT_NE(a->GetVocabulary(), b->GetVocabulary());
}

TEST_P(AvTest, ReplaceVersion2) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  Vocabulary v;
  v.insert(avtestinfo_->k0);
  v.insert(avtestinfo_->k1);
  v.insert(avtestinfo_->k2);

  a->Project(v);
  a->AddEquality(avtestinfo_->k1, avtestinfo_->k2);

  wali::ref_ptr<AV> b = a->Copy();
  b->ReplaceVersion(Version(0), Version(0));
  EXPECT_EQ(*a, *b);

  b->ReplaceVersion(Version(1), Version(2));
  EXPECT_EQ(a->GetVocabulary(), b->GetVocabulary());
}

TEST_P(AvTest, ReplaceVersions) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  a->AddEquality(*(avtestinfo_->av->GetVocabulary().begin()), *((avtestinfo_->av->GetVocabulary().begin())++));

  wali::ref_ptr<AV> b = a->Copy();
  std::map<Version, Version> m;
  m.insert(std::pair<Version, Version>(Version(0), Version(0)));
  m.insert(std::pair<Version, Version>(Version(1), Version(1)));
  b->ReplaceVersions(m);
  EXPECT_EQ(*a, *b);
}

TEST_P(AvTest, RemoveVocabulary) {
  wali::ref_ptr<AV> a = avtestinfo_->av->Top();
  a->AddEquality(avtestinfo_->k0, avtestinfo_->k2);

  Vocabulary v;

  wali::ref_ptr<AV> b = a->Copy();
  b->RemoveVocabulary(v);
  EXPECT_EQ(6u, b->NumVars());
  EXPECT_EQ(*a, *b);

  wali::ref_ptr<AV> c = a->Copy();
  c->RemoveDimension(avtestinfo_->k0);
  EXPECT_EQ(5u, c->NumVars());
  EXPECT_TRUE(c->IsTop());

  Vocabulary v2;
  v2.insert(avtestinfo_->k3);

  wali::ref_ptr<AV> d = a->Copy();
  d->RemoveVocabulary(v2);
  EXPECT_EQ(5u, d->NumVars());
}

class PointsetPowersetAvTest : public ::testing::Test {
public:
  PointsetPowersetAvTest() {
    ppavtestinfo_ = PointsetPowersetAvTest::GetAvTestInfo();
    ppavtestinfo_->Initialize();
  }

  ~PointsetPowersetAvTest() {
    ppavtestinfo_->Finalize();
  }

  static std::shared_ptr<AvTestInfo> GetAvTestInfo() {
    std::shared_ptr<AvTestInfo> ret = std::make_shared<AvTestInfo>();
    Vocabulary v;

    ret->k0 = DimensionKey("x", 0, utils::thirty_two);
    v.insert(ret->k0);

    ret->k1 = DimensionKey("y", 0, utils::thirty_two);
    v.insert(ret->k1);

    ret->k2 = DimensionKey("z", 0, utils::thirty_two);
    v.insert(ret->k2);

    ret->k3 = DimensionKey("x", 1, utils::thirty_two);
    v.insert(ret->k3);

    ret->k4 = DimensionKey("y", 1, utils::thirty_two);
    v.insert(ret->k4);

    ret->k5 = DimensionKey("z", 1, utils::thirty_two);
    v.insert(ret->k5);

    ret->k6 = DimensionKey("w", 0, utils::thirty_two);
    //v.insert(ret->k6);

    ret->k7 = DimensionKey("w", 1, utils::thirty_two);
    //v.insert(ret->k7);

    wali::ref_ptr<PP_CPOLY_AV> av_poly = new PP_CPOLY_AV(v);
    ret->av = av_poly;

    return ret;
  }

  virtual void SetUp() {
  }
  
  wali::ref_ptr<AV> ma() {
    wali::ref_ptr<PP_CPOLY_AV> ret = new PP_CPOLY_AV(ppavtestinfo_->av->GetVocabulary());
    ret->AddEquality(ppavtestinfo_->k0, ppavtestinfo_->k3);
    ret->AddEquality(ppavtestinfo_->k1, ppavtestinfo_->k5);
    ret->AddEquality(ppavtestinfo_->k2, ppavtestinfo_->k5);
    return ret;
  }

  wali::ref_ptr<AV> mb() {
    wali::ref_ptr<PP_CPOLY_AV> ret = new PP_CPOLY_AV(ppavtestinfo_->av->GetVocabulary());
    ret->AddEquality(ppavtestinfo_->k0, ppavtestinfo_->k3);
    ret->AddEquality(ppavtestinfo_->k1, ppavtestinfo_->k4);
    return ret;
  }

  wali::ref_ptr<AV> ma_oct() {
    wali::ref_ptr<PP_OCT_AV> ret = new PP_OCT_AV(ppavtestinfo_->av->GetVocabulary());
    ret->AddEquality(ppavtestinfo_->k0, ppavtestinfo_->k3);
    ret->AddEquality(ppavtestinfo_->k1, ppavtestinfo_->k5);
    ret->AddEquality(ppavtestinfo_->k2, ppavtestinfo_->k5);
    return ret;
  }

  wali::ref_ptr<AV> mb_oct() {
    wali::ref_ptr<PP_OCT_AV> ret = new PP_OCT_AV(ppavtestinfo_->av->GetVocabulary());
    ret->AddEquality(ppavtestinfo_->k0, ppavtestinfo_->k3);
    ret->AddEquality(ppavtestinfo_->k1, ppavtestinfo_->k4);
    return ret;
  }

  std::shared_ptr<AvTestInfo> ppavtestinfo_;
};

// captures the constraints k1+1=k2, -127<=k2<=127, ie the case where overflow does
// not happen
wali::ref_ptr<PP_CPOLY_AV> GetNonOverFlowBehaviour_8bit_k2_is_k1plus1(DimensionKey k1, DimensionKey k2) {
  Vocabulary k1_k2;
  k1_k2.insert(k1);
  k1_k2.insert(k2);

  wali::ref_ptr<PP_CPOLY_AV> p2 = new PP_CPOLY_AV(k1_k2);
  PP_CPOLY_AV::linexp_type lhs_c1_p2_coeffs, rhs_c1_p2_coeffs;
  lhs_c1_p2_coeffs.insert(std::make_pair(k1, 1));
  PP_CPOLY_AV::affexp_type lhs_c1_p2(lhs_c1_p2_coeffs, 1);
  rhs_c1_p2_coeffs.insert(std::make_pair(k2, 1));
  PP_CPOLY_AV::affexp_type rhs_c1_p2(rhs_c1_p2_coeffs, 0);
  p2->AddConstraint(lhs_c1_p2, rhs_c1_p2, PP_CPOLY_AV::EQ); // Adding constraint (k1 + 1 = k2)


  PP_CPOLY_AV::linexp_type lhs_c2_p2_coeffs, rhs_c2_p2_coeffs;
  PP_CPOLY_AV::affexp_type lhs_c2_p2(lhs_c2_p2_coeffs, -127);
  rhs_c2_p2_coeffs.insert(std::make_pair(k2, 1));
  PP_CPOLY_AV::affexp_type rhs_c2_p2(rhs_c2_p2_coeffs, 0);
  p2->AddConstraint(lhs_c2_p2, rhs_c2_p2, PP_CPOLY_AV::LE); // Adding constraint (-127 <= k2)

  PP_CPOLY_AV::linexp_type lhs_c3_p2_coeffs, rhs_c3_p2_coeffs;
  PP_CPOLY_AV::affexp_type lhs_c3_p2(lhs_c3_p2_coeffs, 127);
  rhs_c3_p2_coeffs.insert(std::make_pair(k2, 1));
  PP_CPOLY_AV::affexp_type rhs_c3_p2(rhs_c3_p2_coeffs, 0);
  p2->AddConstraint(lhs_c3_p2, rhs_c3_p2, PP_CPOLY_AV::GE); // Adding constraint (1 <= k2)
  return p2;
}

// captures the constraints k1+1=k2, -2^31+1<=k2<=2^31-1, ie the case where overflow does
// not happen
wali::ref_ptr<PP_CPOLY_AV> GetNonOverFlowBehaviourk2_is_k1plus1(DimensionKey k1, DimensionKey k2) {
  Vocabulary k1_k2;
  k1_k2.insert(k1);
  k1_k2.insert(k2);

  wali::ref_ptr<PP_CPOLY_AV> p2 = new PP_CPOLY_AV(k1_k2);
  PP_CPOLY_AV::linexp_type lhs_c1_p2_coeffs, rhs_c1_p2_coeffs;
  lhs_c1_p2_coeffs.insert(std::make_pair(k1, 1));
  PP_CPOLY_AV::affexp_type lhs_c1_p2 = std::make_pair(lhs_c1_p2_coeffs, 1);
  rhs_c1_p2_coeffs.insert(std::make_pair(k2, 1));
  PP_CPOLY_AV::affexp_type rhs_c1_p2 = std::make_pair(rhs_c1_p2_coeffs, 0);
  p2->AddConstraint(lhs_c1_p2, rhs_c1_p2, PP_CPOLY_AV::EQ); // Adding constraint (k1 + 1 = k2)


  PP_CPOLY_AV::linexp_type lhs_c2_p2_coeffs, rhs_c2_p2_coeffs;
  PP_CPOLY_AV::affexp_type lhs_c2_p2 = std::make_pair(lhs_c2_p2_coeffs, 0x80000001);
  rhs_c2_p2_coeffs.insert(std::make_pair(k2, 1));
  PP_CPOLY_AV::affexp_type rhs_c2_p2 = std::make_pair(rhs_c2_p2_coeffs, 0);
  p2->AddConstraint(lhs_c2_p2, rhs_c2_p2, PP_CPOLY_AV::LE); // Adding constraint (-2^31-1 <= k2)


  PP_CPOLY_AV::linexp_type lhs_c3_p2_coeffs, rhs_c3_p2_coeffs;
  PP_CPOLY_AV::affexp_type lhs_c3_p2 = std::make_pair(lhs_c3_p2_coeffs, 0x7FFFFFFF);
  rhs_c3_p2_coeffs.insert(std::make_pair(k2, 1));
  PP_CPOLY_AV::affexp_type rhs_c3_p2 = std::make_pair(rhs_c3_p2_coeffs, 0);
  p2->AddConstraint(lhs_c3_p2, rhs_c3_p2, PP_CPOLY_AV::GE); // Adding constraint (2^31-1 <= k2)
  return p2;
}

// p3 is the point (MAX_INT,MIN_INT), ie the overwrapping case
wali::ref_ptr<PP_CPOLY_AV> GetOverFlowBehaviour_8bit_k2_is_k1plus1(DimensionKey k1, DimensionKey k2) {
  Vocabulary k1_k2;
  k1_k2.insert(k1);
  k1_k2.insert(k2);

  wali::ref_ptr<PP_CPOLY_AV> p3 = new PP_CPOLY_AV(k1_k2);
  PP_CPOLY_AV::linexp_type lhs_c1_p3_coeffs, rhs_c1_p3_coeffs;
  lhs_c1_p3_coeffs.insert(std::make_pair(k1, 1));
  PP_CPOLY_AV::affexp_type lhs_c1_p3 = std::make_pair(lhs_c1_p3_coeffs, 0);
  PP_CPOLY_AV::affexp_type rhs_c1_p3 = std::make_pair(rhs_c1_p3_coeffs, 127);
  p3->AddConstraint(lhs_c1_p3, rhs_c1_p3, PP_CPOLY_AV::EQ); // Adding constraint (k1 = 127)

  PP_CPOLY_AV::linexp_type lhs_c2_p3_coeffs, rhs_c2_p3_coeffs;
  lhs_c2_p3_coeffs.insert(std::make_pair(k2, 1));
  PP_CPOLY_AV::affexp_type lhs_c2_p3 = std::make_pair(lhs_c2_p3_coeffs, 0);
  PP_CPOLY_AV::affexp_type rhs_c2_p3 = std::make_pair(rhs_c2_p3_coeffs, -128);
  p3->AddConstraint(lhs_c2_p3, rhs_c2_p3, PP_CPOLY_AV::EQ); // Adding constraint (k2 = -128)
  return p3;
}

// p3 is the point (MAX_INT,MIN_INT), ie the overwrapping case
wali::ref_ptr<PP_CPOLY_AV> GetOverFlowBehaviourk2_is_k1plus1(DimensionKey k1, DimensionKey k2) {
  Vocabulary k1_k2;
  k1_k2.insert(k1);
  k1_k2.insert(k2);

  wali::ref_ptr<PP_CPOLY_AV> p3 = new PP_CPOLY_AV(k1_k2);
  PP_CPOLY_AV::linexp_type lhs_c1_p3_coeffs, rhs_c1_p3_coeffs;
  lhs_c1_p3_coeffs.insert(std::make_pair(k1, 1));
  PP_CPOLY_AV::affexp_type lhs_c1_p3 = std::make_pair(lhs_c1_p3_coeffs, 0);
  PP_CPOLY_AV::affexp_type rhs_c1_p3 = std::make_pair(rhs_c1_p3_coeffs, 0x7FFFFFFF);
  p3->AddConstraint(lhs_c1_p3, rhs_c1_p3, PP_CPOLY_AV::EQ); // Adding constraint (k1 = MAX_INT)

  PP_CPOLY_AV::linexp_type lhs_c2_p3_coeffs, rhs_c2_p3_coeffs;
  lhs_c2_p3_coeffs.insert(std::make_pair(k2, 1));
  PP_CPOLY_AV::affexp_type lhs_c2_p3 = std::make_pair(lhs_c2_p3_coeffs, 0);
  PP_CPOLY_AV::affexp_type rhs_c2_p3 = std::make_pair(rhs_c2_p3_coeffs, 0x80000000);
  p3->AddConstraint(lhs_c2_p3, rhs_c2_p3, PP_CPOLY_AV::EQ); // Adding constraint (k2 = MIN_INT)
  return p3;
}

//Constructor
TEST_F(PointsetPowersetAvTest, ConstructorCreatesTop) {
  Vocabulary v;
  v.insert(ppavtestinfo_->k0);
  v.insert(ppavtestinfo_->k1);
  PP_CPOLY_AV av(v);
  EXPECT_TRUE(av.IsTop());

  wali::ref_ptr<AV> tp = av.Top();
  PP_CPOLY_AV* tp_pp = static_cast<PP_CPOLY_AV*>(tp.get_ptr());

  EXPECT_EQ(av, *tp_pp);
}

TEST_F(PointsetPowersetAvTest, CopyConstructor) {
  Vocabulary v;
  v.insert(ppavtestinfo_->k0);
  v.insert(ppavtestinfo_->k1);
  wali::ref_ptr<AV> c = ma();

  PP_CPOLY_AV* c_i = static_cast<PP_CPOLY_AV*>(c.get_ptr());

  PP_CPOLY_AV copy(*c_i);
  EXPECT_EQ(*c, copy);
}

TEST_F(PointsetPowersetAvTest, CommuteJoin) {
  wali::ref_ptr<AV> aJoinB = ma();
  aJoinB->Join(mb());
  PP_CPOLY_AV* aJoinB_pp = static_cast<PP_CPOLY_AV*>(aJoinB.get_ptr());

  wali::ref_ptr<AV> bJoinA = mb();
  bJoinA->Join(ma());
  PP_CPOLY_AV* bJoinA_pp = static_cast<PP_CPOLY_AV*>(bJoinA.get_ptr());

  ma()->print(std::cout << "\nma():");
  mb()->print(std::cout << "\nmb():");

  aJoinB_pp->print(std::cout << "\naJoinB_pp:");
  bJoinA_pp->print(std::cout << "\nbJoinA_pp:");

  EXPECT_EQ(*aJoinB_pp, *bJoinA_pp);
  EXPECT_TRUE(aJoinB->Equal(bJoinA));
  EXPECT_EQ(aJoinB_pp->num_disjuncts(), 1u);
}

TEST_F(PointsetPowersetAvTest, CommuteJoinkis2) {
  PP_CPOLY_AV::max_disjunctions = 2;
  wali::ref_ptr<AV> aJoinB = ma()->Copy();
  aJoinB->Join(mb());
  PP_CPOLY_AV* aJoinB_pp = static_cast<PP_CPOLY_AV*>(aJoinB.get_ptr());

  wali::ref_ptr<AV> bJoinA = mb()->Copy();
  bJoinA->Join(ma());
  PP_CPOLY_AV* bJoinA_pp = static_cast<PP_CPOLY_AV*>(bJoinA.get_ptr());

  EXPECT_EQ(*aJoinB_pp, *bJoinA_pp);
  EXPECT_TRUE(aJoinB->Equal(bJoinA));
  EXPECT_EQ(aJoinB_pp->num_disjuncts(), 2u);
  PP_CPOLY_AV::max_disjunctions = 1;
}

TEST_F(PointsetPowersetAvTest, Join2Interval) {
  Vocabulary v;
  v.insert(ppavtestinfo_->k0);

  PP_CPOLY_AV::max_disjunctions = 1;
  wali::ref_ptr<PP_CPOLY_AV> i1 = new PP_CPOLY_AV(v);
  PP_CPOLY_AV::linexp_type k_le; k_le.insert(PP_CPOLY_AV::linexp_type::value_type(ppavtestinfo_->k0, mpz_class(1)));
  i1->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -3), PP_CPOLY_AV::OpType::GE); // k >= 3
  i1->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -5), PP_CPOLY_AV::OpType::LE); // k <= 5

  wali::ref_ptr<PP_CPOLY_AV> i2 = new PP_CPOLY_AV(v);
  i2->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -6), PP_CPOLY_AV::OpType::GE); // k >= 6
  i2->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -9), PP_CPOLY_AV::OpType::LE); // k <= 9

  i1->print(std::cout << "\ni1:");
  i2->print(std::cout << "\ni2:");

  wali::ref_ptr<PP_CPOLY_AV> exp_i1_j_i2 = new PP_CPOLY_AV(v);
  exp_i1_j_i2->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -3), PP_CPOLY_AV::OpType::GE); // k >= 3
  exp_i1_j_i2->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -9), PP_CPOLY_AV::OpType::LE); // k <= 9

  exp_i1_j_i2->print(std::cout << "\nexp_i1_j_i2:");
 
  wali::ref_ptr<AV> i1_j_i2_av = i1->Copy();
  i1_j_i2_av->Join(i2);
  PP_CPOLY_AV* i1_j_i2 = static_cast<PP_CPOLY_AV*>(i1_j_i2_av.get_ptr());
  i1_j_i2->print(std::cout << "\ni1_j_i2:");

  wali::ref_ptr<AV> i2_j_i1_av = i2->Copy();
  i2_j_i1_av->Join(i1);
  PP_CPOLY_AV* i2_j_i1 = static_cast<PP_CPOLY_AV*>(i2_j_i1_av.get_ptr());
  i2_j_i1->print(std::cout << "\ni2_j_i1:");

  EXPECT_EQ(*i1_j_i2, *i2_j_i1);
  EXPECT_NE(*i1_j_i2, *i2);
  EXPECT_NE(*i1_j_i2, *i1);
  EXPECT_EQ(*exp_i1_j_i2, *i1_j_i2);
  EXPECT_TRUE(i1_j_i2_av->Equal(i2_j_i1_av));
  EXPECT_EQ(i1_j_i2->num_disjuncts(), 1u);
}

TEST_F(PointsetPowersetAvTest, Join3Intervalkis2) {
  Vocabulary v;
  v.insert(ppavtestinfo_->k0);

  PP_CPOLY_AV::max_disjunctions = 2;
  wali::ref_ptr<PP_CPOLY_AV> i1 = new PP_CPOLY_AV(v);
  PP_CPOLY_AV::linexp_type k_le; k_le.insert(PP_CPOLY_AV::linexp_type::value_type(ppavtestinfo_->k0, mpz_class(1)));
  i1->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -3), PP_CPOLY_AV::OpType::GE); // k >= 3
  i1->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -5), PP_CPOLY_AV::OpType::LE); // k <= 5

  wali::ref_ptr<PP_CPOLY_AV> i2 = new PP_CPOLY_AV(v);
  i2->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -6), PP_CPOLY_AV::OpType::GE); // k >= 6
  i2->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -9), PP_CPOLY_AV::OpType::LE); // k <= 9

  wali::ref_ptr<PP_CPOLY_AV> i3 = new PP_CPOLY_AV(v);
  i3->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -12), PP_CPOLY_AV::OpType::GE); // k >= 12
  i3->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -15), PP_CPOLY_AV::OpType::LE); // k <= 15

  i1->print(std::cout << "\ni1:");
  i2->print(std::cout << "\ni2:");
  i3->print(std::cout << "\ni3:");

  wali::ref_ptr<PP_CPOLY_AV> exp_i1_j_i2_j_i3 = new PP_CPOLY_AV(v);
  exp_i1_j_i2_j_i3->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -3), PP_CPOLY_AV::OpType::GE); // k >= 3
  exp_i1_j_i2_j_i3->AddConstraintNorhs(PP_CPOLY_AV::affexp_type(k_le, -9), PP_CPOLY_AV::OpType::LE); // k <= 9
  exp_i1_j_i2_j_i3->AddDisjunct(i3->GetDisjunct(0));

  exp_i1_j_i2_j_i3->print(std::cout << "\nexp_i1_j_i2:");
 
  wali::ref_ptr<AV> i1_j_i2_j_i3_av = i1->Copy();
  i1_j_i2_j_i3_av->Join(i2);
  i1_j_i2_j_i3_av->Join(i3);
  PP_CPOLY_AV* i1_j_i2_j_i3 = static_cast<PP_CPOLY_AV*>(i1_j_i2_j_i3_av.get_ptr());
  i1_j_i2_j_i3->print(std::cout << "\ni1_j_i2_j_i3:");

  EXPECT_NE(*i1_j_i2_j_i3, *i1);
  EXPECT_NE(*i1_j_i2_j_i3, *i2);
  EXPECT_NE(*i1_j_i2_j_i3, *i3);
  EXPECT_EQ(*exp_i1_j_i2_j_i3, *i1_j_i2_j_i3);
  EXPECT_EQ(i1_j_i2_j_i3->num_disjuncts(), 2u);
  PP_CPOLY_AV::max_disjunctions = 1;
}

TEST_F(PointsetPowersetAvTest, CommuteMeet) {
  wali::ref_ptr<AV> aMeetB = ma()->Copy();
  aMeetB->Meet(mb());
  PP_CPOLY_AV* aMeetB_poly = static_cast<PP_CPOLY_AV*>(aMeetB.get_ptr());

  wali::ref_ptr<AV> bMeetA = mb()->Copy();
  bMeetA->Meet(ma());
  PP_CPOLY_AV* bMeetA_poly = static_cast<PP_CPOLY_AV*>(bMeetA.get_ptr());

  EXPECT_EQ(*aMeetB_poly, *bMeetA_poly);
  EXPECT_TRUE(aMeetB->Equal(bMeetA));
}

TEST_F(PointsetPowersetAvTest, CommuteMeetkis2) {
  PP_CPOLY_AV::max_disjunctions = 2;
  wali::ref_ptr<AV> aMeetB = ma()->Copy();
  aMeetB->Meet(mb());
  PP_CPOLY_AV* aMeetB_poly = static_cast<PP_CPOLY_AV*>(aMeetB.get_ptr());

  wali::ref_ptr<AV> bMeetA = mb()->Copy();
  bMeetA->Meet(ma());
  PP_CPOLY_AV* bMeetA_poly = static_cast<PP_CPOLY_AV*>(bMeetA.get_ptr());

  EXPECT_EQ(*aMeetB_poly, *bMeetA_poly);
  EXPECT_TRUE(aMeetB->Equal(bMeetA));
  PP_CPOLY_AV::max_disjunctions = 2;
}


//Constructor
TEST_F(PointsetPowersetAvTest, ConstructorCreatesTopOct) {
  Vocabulary v;
  v.insert(ppavtestinfo_->k0);
  v.insert(ppavtestinfo_->k1);
  PP_OCT_AV av(v);
  EXPECT_TRUE(av.IsTop());

  wali::ref_ptr<AV> tp = av.Top();
  PP_OCT_AV* tp_pp = static_cast<PP_OCT_AV*>(tp.get_ptr());

  EXPECT_EQ(av, *tp_pp);
}

TEST_F(PointsetPowersetAvTest, CopyConstructorOct) {
  Vocabulary v;
  v.insert(ppavtestinfo_->k0);
  v.insert(ppavtestinfo_->k1);
  wali::ref_ptr<AV> c = ma_oct();
  PP_OCT_AV* c_i = static_cast<PP_OCT_AV*>(c.get_ptr());

  PP_OCT_AV copy(*c_i);
  EXPECT_EQ(*c, copy);
}

TEST_F(PointsetPowersetAvTest, CommuteJoinOct) {
  wali::ref_ptr<AV> aJoinB = ma_oct()->Copy();
  aJoinB->Join(mb_oct());
  PP_OCT_AV* aJoinB_pp = static_cast<PP_OCT_AV*>(aJoinB.get_ptr());

  wali::ref_ptr<AV> bJoinA = mb_oct()->Copy();
  bJoinA->Join(ma_oct());
  PP_OCT_AV* bJoinA_pp = static_cast<PP_OCT_AV*>(bJoinA.get_ptr());

  ma_oct()->print(std::cout << "\nma_oct():");
  mb_oct()->print(std::cout << "\nmb_oct():");

  aJoinB_pp->print(std::cout << "\naJoinB_pp:");
  bJoinA_pp->print(std::cout << "\nbJoinA_pp:");

  EXPECT_EQ(*aJoinB_pp, *bJoinA_pp);
  EXPECT_TRUE(aJoinB->Equal(bJoinA));
  EXPECT_EQ(aJoinB_pp->num_disjuncts(), 1u);
}

TEST_F(PointsetPowersetAvTest, CommuteJoinkis2Oct) {
  PP_OCT_AV::max_disjunctions = 2;
  wali::ref_ptr<AV> aJoinB = ma_oct()->Copy();
  aJoinB->Join(mb_oct());
  PP_OCT_AV* aJoinB_pp = static_cast<PP_OCT_AV*>(aJoinB.get_ptr());

  wali::ref_ptr<AV> bJoinA = mb_oct()->Copy();
  bJoinA->Join(ma_oct());
  PP_OCT_AV* bJoinA_pp = static_cast<PP_OCT_AV*>(bJoinA.get_ptr());

  EXPECT_EQ(*aJoinB_pp, *bJoinA_pp);
  EXPECT_TRUE(aJoinB->Equal(bJoinA));
  EXPECT_EQ(aJoinB_pp->num_disjuncts(), 2u);
  PP_OCT_AV::max_disjunctions = 1;
}

TEST_F(PointsetPowersetAvTest, Join2IntervalOct) {
  Vocabulary v;
  v.insert(ppavtestinfo_->k0);

  PP_OCT_AV::max_disjunctions = 1;
  wali::ref_ptr<PP_OCT_AV> i1 = new PP_OCT_AV(v);
  PP_OCT_AV::linexp_type k_le; k_le.insert(PP_OCT_AV::linexp_type::value_type(ppavtestinfo_->k0, mpz_class(1)));
  i1->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -3), PP_OCT_AV::OpType::GE); // k >= 3
  i1->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -5), PP_OCT_AV::OpType::LE); // k <= 5

  wali::ref_ptr<PP_OCT_AV> i2 = new PP_OCT_AV(v);
  i2->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -6), PP_OCT_AV::OpType::GE); // k >= 6
  i2->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -9), PP_OCT_AV::OpType::LE); // k <= 9

  i1->print(std::cout << "\ni1:");
  i2->print(std::cout << "\ni2:");

  wali::ref_ptr<PP_OCT_AV> exp_i1_j_i2 = new PP_OCT_AV(v);
  exp_i1_j_i2->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -3), PP_OCT_AV::OpType::GE); // k >= 3
  exp_i1_j_i2->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -9), PP_OCT_AV::OpType::LE); // k <= 9

  exp_i1_j_i2->print(std::cout << "\nexp_i1_j_i2:");
 
  wali::ref_ptr<AV> i1_j_i2_av = i1->Copy();
  i1_j_i2_av->Join(i2);
  PP_OCT_AV* i1_j_i2 = static_cast<PP_OCT_AV*>(i1_j_i2_av.get_ptr());
  i1_j_i2->print(std::cout << "\ni1_j_i2:");

  wali::ref_ptr<AV> i2_j_i1_av = i2->Copy();
  i2_j_i1_av->Join(i1);
  PP_OCT_AV* i2_j_i1 = static_cast<PP_OCT_AV*>(i2_j_i1_av.get_ptr());
  i2_j_i1->print(std::cout << "\ni2_j_i1:");

  EXPECT_EQ(*i1_j_i2, *i2_j_i1);
  EXPECT_NE(*i1_j_i2, *i2);
  EXPECT_NE(*i1_j_i2, *i1);
  EXPECT_EQ(*exp_i1_j_i2, *i1_j_i2);
  EXPECT_TRUE(i1_j_i2_av->Equal(i2_j_i1_av));
  EXPECT_EQ(i1_j_i2->num_disjuncts(), 1u);
}

TEST_F(PointsetPowersetAvTest, Join3Intervalkis2Oct) {
  Vocabulary v;
  v.insert(ppavtestinfo_->k0);

  PP_OCT_AV::max_disjunctions = 2;
  wali::ref_ptr<PP_OCT_AV> i1 = new PP_OCT_AV(v);
  PP_OCT_AV::linexp_type k_le; k_le.insert(PP_OCT_AV::linexp_type::value_type(ppavtestinfo_->k0, mpz_class(1)));
  i1->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -3), PP_OCT_AV::OpType::GE); // k >= 3
  i1->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -5), PP_OCT_AV::OpType::LE); // k <= 5

  wali::ref_ptr<PP_OCT_AV> i2 = new PP_OCT_AV(v);
  i2->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -6), PP_OCT_AV::OpType::GE); // k >= 6
  i2->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -9), PP_OCT_AV::OpType::LE); // k <= 9

  wali::ref_ptr<PP_OCT_AV> i3 = new PP_OCT_AV(v);
  i3->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -12), PP_OCT_AV::OpType::GE); // k >= 12
  i3->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -15), PP_OCT_AV::OpType::LE); // k <= 15

  i1->print(std::cout << "\ni1:");
  i2->print(std::cout << "\ni2:");
  i3->print(std::cout << "\ni3:");

  wali::ref_ptr<PP_OCT_AV> exp_i1_j_i2_j_i3 = new PP_OCT_AV(v);
  exp_i1_j_i2_j_i3->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -3), PP_OCT_AV::OpType::GE); // k >= 3
  exp_i1_j_i2_j_i3->AddConstraintNorhs(PP_OCT_AV::affexp_type(k_le, -9), PP_OCT_AV::OpType::LE); // k <= 9
  exp_i1_j_i2_j_i3->AddDisjunct(i3->GetDisjunct(0));

  exp_i1_j_i2_j_i3->print(std::cout << "\nexp_i1_j_i2:");
 
  wali::ref_ptr<AV> i1_j_i2_j_i3_av = i1->Copy();
  i1_j_i2_j_i3_av->Join(i2);
  i1_j_i2_j_i3_av->Join(i3);
  PP_OCT_AV* i1_j_i2_j_i3 = static_cast<PP_OCT_AV*>(i1_j_i2_j_i3_av.get_ptr());
  i1_j_i2_j_i3->print(std::cout << "\ni1_j_i2_j_i3:");

  EXPECT_NE(*i1_j_i2_j_i3, *i1);
  EXPECT_NE(*i1_j_i2_j_i3, *i2);
  EXPECT_NE(*i1_j_i2_j_i3, *i3);
  EXPECT_EQ(*exp_i1_j_i2_j_i3, *i1_j_i2_j_i3);
  EXPECT_EQ(i1_j_i2_j_i3->num_disjuncts(), 2u);
  PP_OCT_AV::max_disjunctions = 1;
}

TEST_F(PointsetPowersetAvTest, CommuteMeetOct) {
  wali::ref_ptr<AV> aMeetB = ma_oct()->Copy();
  aMeetB->Meet(mb_oct());
  PP_OCT_AV* aMeetB_poly = static_cast<PP_OCT_AV*>(aMeetB.get_ptr());

  wali::ref_ptr<AV> bMeetA = mb_oct()->Copy();
  bMeetA->Meet(ma_oct());
  PP_OCT_AV* bMeetA_poly = static_cast<PP_OCT_AV*>(bMeetA.get_ptr());

  EXPECT_EQ(*aMeetB_poly, *bMeetA_poly);
  EXPECT_TRUE(aMeetB->Equal(bMeetA));
}

TEST_F(PointsetPowersetAvTest, CommuteMeetkis2Oct) {
  PP_OCT_AV::max_disjunctions = 2;
  wali::ref_ptr<AV> aMeetB = ma_oct()->Copy();
  aMeetB->Meet(mb_oct());
  PP_OCT_AV* aMeetB_poly = static_cast<PP_OCT_AV*>(aMeetB.get_ptr());

  wali::ref_ptr<AV> bMeetA = mb_oct()->Copy();
  bMeetA->Meet(ma_oct());
  PP_OCT_AV* bMeetA_poly = static_cast<PP_OCT_AV*>(bMeetA.get_ptr());

  EXPECT_EQ(*aMeetB_poly, *bMeetA_poly);
  EXPECT_TRUE(aMeetB->Equal(bMeetA));
  PP_OCT_AV::max_disjunctions = 2;
}


static std::shared_ptr<AvTestInfo> ppavtestinfo;
INSTANTIATE_TEST_CASE_P(Pp, AvTest, ::testing::Values(ppavtestinfo));

int main(int argc, char* argv[]) {
  ppavtestinfo = PointsetPowersetAvTest::GetAvTestInfo();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
