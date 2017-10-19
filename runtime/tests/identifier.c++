#include <functional>
#include <iostream>

#include "runtime/dispatch.h"
#include "gtest/gtest.h"

TEST(IdTest, LambdaIdentical) {
  auto l = [] {};
  EXPECT_EQ(atlas::_::work_type(l), atlas::_::work_type(l));
}

TEST(IdTest, LambdaNotIdentical) {
  auto l = [] {};
  auto r = [] {};
  EXPECT_PRED2(std::not_equal_to<>{}, atlas::_::work_type(l),
               atlas::_::work_type(r));
}

TEST(IdTest, FunctionObjectIdentical) {
  struct foo {
  } l;
  EXPECT_EQ(atlas::_::work_type(l), atlas::_::work_type(l));
  EXPECT_EQ(atlas::_::work_type(l), std::type_index(typeid(foo)).hash_code());
}

TEST(IdTest, FunctionObjectNotIdentical) {
  struct {
  } l;
  struct {
  } r;
  EXPECT_PRED2(std::not_equal_to<>{}, atlas::_::work_type(l),
               atlas::_::work_type(r));
}

static void function() {}

int main(int argc, char **argv) {
  std::cout << std::hex << atlas::_::work_type(function) << std::endl;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
