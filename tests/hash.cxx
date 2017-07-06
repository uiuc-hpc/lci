#include <gtest/gtest.h>

#include "lc/hashtable.h"

TEST(HASH, InitFini)
{
  lc_hash* h = nullptr;
  lc_hash_create(&h);
  ASSERT_NE(h, nullptr);
  lc_hash_destroy(h);
}

TEST(HASH, InsertServerFirst)
{
  lc_hash* h;
  lc_hash_create(&h);
  lc_value v = 1;
  lc_key k = 1;
  ASSERT_TRUE(lc_hash_insert(h, k, &v, SERVER));
  ASSERT_EQ(v, 1);
  v = 2;
  ASSERT_TRUE(lc_hash_insert(h, k, &v, SERVER));
  ASSERT_EQ(v, 2);
  v = -3;
  ASSERT_FALSE(lc_hash_insert(h, k, &v, CLIENT));
  ASSERT_TRUE(v == 1 || v == 2);
  v = -3;
  ASSERT_FALSE(lc_hash_insert(h, k, &v, CLIENT));
  ASSERT_TRUE(v == 1 || v == 2);
  v = -3;
  ASSERT_TRUE(lc_hash_insert(h, k, &v, CLIENT));
  ASSERT_EQ(v, -3);
  lc_hash_destroy(h);
}

TEST(HASH, InsertClientFirst)
{
  lc_hash* h;
  lc_hash_create(&h);
  lc_value v = 1;
  lc_key k = 1;
  ASSERT_TRUE(lc_hash_insert(h, k, &v, CLIENT));
  ASSERT_EQ(v, 1);
  v = 2;
  ASSERT_TRUE(lc_hash_insert(h, k, &v, CLIENT));
  ASSERT_EQ(v, 2);
  v = -3;
  ASSERT_FALSE(lc_hash_insert(h, k, &v, SERVER));
  ASSERT_TRUE(v == 1 || v == 2);
  v = -3;
  ASSERT_FALSE(lc_hash_insert(h, k, &v, SERVER));
  ASSERT_TRUE(v == 1 || v == 2);
  v = -3;
  ASSERT_TRUE(lc_hash_insert(h, k, &v, SERVER));
  ASSERT_EQ(v, -3);
  lc_hash_destroy(h);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
