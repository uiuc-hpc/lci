#include "lcii_register.h"

TEST(REG, Basic)
{
  LCII_register_t reg;
  ASSERT_EQ(LCII_register_init(&reg, 16), LCI_OK);
  std::vector<LCII_reg_key_t> keys;
  for (int i = 0; i < 100; ++i) {
    int *p = (int*)malloc(sizeof(int));
    *p = i;
    LCII_reg_key_t key;
    ASSERT_EQ(LCII_register_put(reg, (LCII_reg_value_t)p, &key), LCI_OK);
    keys.push_back(key);
  }
  for (int i = 0; i < 100; ++i) {
    LCII_reg_key_t key = keys[i];
    int *p = (int*) LCII_register_remove(reg, key);
    ASSERT_EQ(*p, i);
  }
  ASSERT_EQ(LCII_register_fini(&reg), LCI_OK);
}
