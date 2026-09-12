// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

TEST(GetEnvOr, boolean)
{
  constexpr const char* env = "LCI_TEST_BOOLEAN_ENV";

  unsetenv(env);
  EXPECT_TRUE(lci::get_env_or(env, true));
  EXPECT_FALSE(lci::get_env_or(env, false));

  setenv(env, "ON", 1);
  EXPECT_TRUE(lci::get_env_or(env, false));
  setenv(env, "true", 1);
  EXPECT_TRUE(lci::get_env_or(env, false));
  setenv(env, "yes", 1);
  EXPECT_TRUE(lci::get_env_or(env, false));
  setenv(env, "1", 1);
  EXPECT_TRUE(lci::get_env_or(env, false));

  setenv(env, "OFF", 1);
  EXPECT_FALSE(lci::get_env_or(env, true));
  setenv(env, "false", 1);
  EXPECT_FALSE(lci::get_env_or(env, true));
  setenv(env, "no", 1);
  EXPECT_FALSE(lci::get_env_or(env, true));
  setenv(env, "0", 1);
  EXPECT_FALSE(lci::get_env_or(env, true));

  unsetenv(env);
}
