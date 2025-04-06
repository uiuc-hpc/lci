// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace lci
{
namespace bootstrap
{
namespace detail
{
const char* HEX_CHARS = "0123456789abcdef";

uint8_t hexCharToValue(char c)
{
  if ('0' <= c && c <= '9')
    return c - '0';
  else if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  else if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  else
    throw std::invalid_argument("Invalid hex character");
}

uint8_t hexToByte(const char* hex)
{
  return (hexCharToValue(hex[0]) << 4) | hexCharToValue(hex[1]);
}

void encode_value(char* buf_origin, size_t nbytes, char* buf_encoded)
{
  LCI_Assert(LCT_PMI_STRING_LIMIT >= 2 * nbytes + 1,
             "Buffer to store encoded address is too short! Use a higher "
             "ENCODED_LIMIT");
  for (size_t i = 0; i < nbytes; i++) {
    // encode every byte as a hex integer of 2 bytes
    char byte = buf_origin[i];
    buf_encoded[2 * i] = HEX_CHARS[(byte >> 4) & 0x0F];
    buf_encoded[2 * i + 1] = HEX_CHARS[byte & 0x0F];
  }
}

void decode_value(char* buf_encoded, size_t nbytes, char* buf_origin)
{
  size_t nbytes_encoded = strlen(buf_encoded);
  LCI_Assert(nbytes_encoded == 2 * nbytes,
             "Encoded buffer length is not correct! Expected %d, got %d\n",
             2 * nbytes, nbytes_encoded);

  for (size_t i = 0; i < nbytes; i++) {
    // decode every 2 bytes as a hex integer
    buf_origin[i] = hexToByte(buf_encoded + 2 * i);
  }
}
}  // namespace detail

device_t device_to_bootstrap;
int rank_me = -1;
int rank_n = -1;

void initialize()
{
  LCT_pmi_initialize();
  rank_me = LCT_pmi_get_rank();
  rank_n = LCT_pmi_get_size();
}

int get_rank_me() { return rank_me; }

int get_rank_n() { return rank_n; }

void finalize()
{
  device_to_bootstrap = device_t();
  rank_me = -1;
  rank_n = -1;
  LCT_pmi_finalize();
}

void set_device(device_t device) { device_to_bootstrap = device; }

void alltoall(const void* sendbuf, void* recvbuf, size_t size)
{
  static int g_next_round = 0;
  int round = g_next_round++;

  if (device_to_bootstrap.is_empty() ||
      !internal_config::enable_bootstrap_lci) {
    LCI_Log(LOG_INFO, "bootstrap", "Bootstrap round %d with LCT PMI\n", round);
    // use LCT pmi wrapper
    for (int i = 0; i < rank_n; i++) {
      char key[LCT_PMI_STRING_LIMIT];
      char value[LCT_PMI_STRING_LIMIT];
      memset(key, 0, LCT_PMI_STRING_LIMIT);
      memset(value, 0, LCT_PMI_STRING_LIMIT);
      sprintf(key, "LCI_BOOTSTRAP_%d_%d_%d", round, i, rank_me);
      // memcpy(value, (char*)sendbuf + i * size, size);
      detail::encode_value((char*)sendbuf + i * size, size, value);
      LCT_pmi_publish(key, value);
    }
    LCT_pmi_barrier();
    for (int i = 0; i < rank_n; i++) {
      char key[LCT_PMI_STRING_LIMIT];
      char value[LCT_PMI_STRING_LIMIT];
      memset(key, 0, LCT_PMI_STRING_LIMIT);
      memset(value, 0, LCT_PMI_STRING_LIMIT);
      sprintf(key, "LCI_BOOTSTRAP_%d_%d_%d", round, rank_me, i);
      LCT_pmi_getname(i, key, value);
      // memcpy((char*)recvbuf + i * size, value, size);
      detail::decode_value(value, size, (char*)recvbuf + i * size);
    }
  } else {
    // use device to do alltoall
    LCI_Log(LOG_INFO, "bootstrap", "Bootstrap round %d with LCI alltoall\n",
            round);
    alltoall_x(sendbuf, recvbuf, size).device(device_to_bootstrap)();
  }
  LCI_Log(LOG_INFO, "bootstrap", "Bootstrap round %d done\n", round);
}
}  // namespace bootstrap
}  // namespace lci