#include <modbus_decode.h>
#include <unity.h>

using namespace mbdec;

void setUp() {}
void tearDown() {}

static void test_u16_and_s16() {
  const uint16_t regs[] = {0xFFFE};
  TEST_ASSERT_EQUAL_DOUBLE(65534.0, decode(regs, RegType::U16, WordOrder::ABCD, 1.0));
  TEST_ASSERT_EQUAL_DOUBLE(-2.0, decode(regs, RegType::S16, WordOrder::ABCD, 1.0));
}

static void test_scale_is_applied() {
  const uint16_t regs[] = {2301};  // 230.1 V sent as x10
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 230.1, decode(regs, RegType::U16, WordOrder::ABCD, 0.1));
}

static void test_u32_word_orders() {
  const uint16_t abcd[] = {0x0001, 0x0002};
  TEST_ASSERT_EQUAL_DOUBLE(65538.0, decode(abcd, RegType::U32, WordOrder::ABCD, 1.0));
  const uint16_t cdab[] = {0x0002, 0x0001};
  TEST_ASSERT_EQUAL_DOUBLE(65538.0, decode(cdab, RegType::U32, WordOrder::CDAB, 1.0));
}

static void test_s32_negative() {
  const uint16_t regs[] = {0xFFFF, 0xFF38};  // -200
  TEST_ASSERT_EQUAL_DOUBLE(-200.0, decode(regs, RegType::S32, WordOrder::ABCD, 1.0));
}

static void test_f32_ieee754() {
  const uint16_t regs[] = {0x4366, 0x0000};  // 230.0f
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 230.0, decode(regs, RegType::F32, WordOrder::ABCD, 1.0));
  const uint16_t swapped[] = {0x0000, 0x4366};
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 230.0, decode(swapped, RegType::F32, WordOrder::CDAB, 1.0));
}

static void test_plan_merges_close_points() {
  const Point points[] = {
      {"voltage", Function::Input, 0x00, RegType::F32, 1.0, "V"},
      {"current", Function::Input, 0x06, RegType::F32, 1.0, "A"},
      {"power", Function::Input, 0x0C, RegType::F32, 1.0, "W"},
  };
  Block blocks[4];
  const size_t n = planBlocks(points, 3, 8, blocks, 4);
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL(0x00, blocks[0].start);
  TEST_ASSERT_EQUAL(0x0E, blocks[0].count);
}

static void test_plan_splits_on_gap_and_function() {
  const Point points[] = {
      {"voltage", Function::Input, 0x00, RegType::F32, 1.0, "V"},
      {"energy", Function::Input, 0x48, RegType::F32, 1.0, "kWh"},
      {"setpoint", Function::Holding, 0x10, RegType::U16, 1.0, ""},
  };
  Block blocks[4];
  const size_t n = planBlocks(points, 3, 8, blocks, 4);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(0, findBlock(points[0], blocks, n));
  TEST_ASSERT_EQUAL(1, findBlock(points[1], blocks, n));
  TEST_ASSERT_EQUAL(2, findBlock(points[2], blocks, n));
}

static void test_plan_respects_max_registers() {
  const Point points[] = {
      {"a", Function::Holding, 0, RegType::U16, 1.0, ""},
      {"b", Function::Holding, 124, RegType::U32, 1.0, ""},  // would make 126 registers
  };
  Block blocks[2];
  TEST_ASSERT_EQUAL(2, planBlocks(points, 2, 200, blocks, 2));
}

static void test_plan_reports_overflow() {
  const Point points[] = {
      {"a", Function::Holding, 0, RegType::U16, 1.0, ""},
      {"b", Function::Input, 0, RegType::U16, 1.0, ""},
  };
  Block blocks[1];
  TEST_ASSERT_EQUAL(0, planBlocks(points, 2, 0, blocks, 1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_u16_and_s16);
  RUN_TEST(test_scale_is_applied);
  RUN_TEST(test_u32_word_orders);
  RUN_TEST(test_s32_negative);
  RUN_TEST(test_f32_ieee754);
  RUN_TEST(test_plan_merges_close_points);
  RUN_TEST(test_plan_splits_on_gap_and_function);
  RUN_TEST(test_plan_respects_max_registers);
  RUN_TEST(test_plan_reports_overflow);
  return UNITY_END();
}
