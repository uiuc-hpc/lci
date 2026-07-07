// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace lci
{
namespace bootstrap
{
namespace detail
{
const char* HEX_CHARS = "0123456789abcdef";
constexpr size_t PMI_ENCODED_CHUNK_BYTES = (LCT_PMI_STRING_LIMIT - 1) / 2;

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

void encode_value(const char* buf_origin, size_t nbytes, char* buf_encoded)
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

int next_round()
{
  static std::atomic<int> g_next_round(0);
  return g_next_round++;
}

size_t nchunks(size_t size)
{
  return size == 0
             ? 0
             : (size + PMI_ENCODED_CHUNK_BYTES - 1) / PMI_ENCODED_CHUNK_BYTES;
}

size_t chunk_size(size_t size, size_t chunk)
{
  const size_t offset = chunk * PMI_ENCODED_CHUNK_BYTES;
  return std::min(PMI_ENCODED_CHUNK_BYTES, size - offset);
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
  int round = detail::next_round();

  if (device_to_bootstrap.is_empty() ||
      !internal_config::enable_bootstrap_lci) {
    LCI_Log(LOG_INFO, "bootstrap", "Bootstrap round %d with LCT PMI\n", round);
    // use LCT pmi wrapper
    const size_t nchunks = detail::nchunks(size);
    for (int i = 0; i < rank_n; i++) {
      for (size_t chunk = 0; chunk < nchunks; ++chunk) {
        char key[LCT_PMI_STRING_LIMIT];
        char value[LCT_PMI_STRING_LIMIT];
        memset(key, 0, LCT_PMI_STRING_LIMIT);
        memset(value, 0, LCT_PMI_STRING_LIMIT);
        snprintf(key, LCT_PMI_STRING_LIMIT, "LCI_BOOTSTRAP_A2A_%d_%d_%d_%lu",
                 round, i, rank_me, chunk);
        const size_t bytes = detail::chunk_size(size, chunk);
        detail::encode_value(static_cast<const char*>(sendbuf) + i * size +
                                 chunk * detail::PMI_ENCODED_CHUNK_BYTES,
                             bytes, value);
        LCT_pmi_publish(key, value);
      }
    }
    LCT_pmi_barrier();
    for (int i = 0; i < rank_n; i++) {
      for (size_t chunk = 0; chunk < nchunks; ++chunk) {
        char key[LCT_PMI_STRING_LIMIT];
        char value[LCT_PMI_STRING_LIMIT];
        memset(key, 0, LCT_PMI_STRING_LIMIT);
        memset(value, 0, LCT_PMI_STRING_LIMIT);
        snprintf(key, LCT_PMI_STRING_LIMIT, "LCI_BOOTSTRAP_A2A_%d_%d_%d_%lu",
                 round, rank_me, i, chunk);
        LCT_pmi_getname(i, key, value);
        const size_t bytes = detail::chunk_size(size, chunk);
        detail::decode_value(value, bytes,
                             static_cast<char*>(recvbuf) + i * size +
                                 chunk * detail::PMI_ENCODED_CHUNK_BYTES);
      }
    }
  } else {
    // use device to do alltoall
    LCI_Log(LOG_INFO, "bootstrap", "Bootstrap round %d with LCI alltoall\n",
            round);
    alltoall_x(sendbuf, recvbuf, size).device(device_to_bootstrap)();
    wait_drained_x().device(device_to_bootstrap)();
  }
  LCI_Log(LOG_INFO, "bootstrap", "Bootstrap round %d done\n", round);
}

void broadcast(const void* sendbuf, void* recvbuf, size_t size, int root)
{
  int round = detail::next_round();
  LCI_Assert(root >= 0 && root < rank_n, "Invalid broadcast root %d\n", root);
  LCI_Log(LOG_INFO, "bootstrap", "Bootstrap broadcast round %d with LCT PMI\n",
          round);
  const size_t nchunks = detail::nchunks(size);
  if (rank_me == root) {
    if (recvbuf != sendbuf && size > 0) memcpy(recvbuf, sendbuf, size);
    for (size_t chunk = 0; chunk < nchunks; ++chunk) {
      char key[LCT_PMI_STRING_LIMIT];
      char value[LCT_PMI_STRING_LIMIT];
      memset(key, 0, LCT_PMI_STRING_LIMIT);
      memset(value, 0, LCT_PMI_STRING_LIMIT);
      snprintf(key, LCT_PMI_STRING_LIMIT, "LCI_BOOTSTRAP_BCAST_%d_%d_%lu",
               round, root, chunk);
      const size_t bytes = detail::chunk_size(size, chunk);
      detail::encode_value(static_cast<const char*>(sendbuf) +
                               chunk * detail::PMI_ENCODED_CHUNK_BYTES,
                           bytes, value);
      LCT_pmi_publish(key, value);
    }
  }
  LCT_pmi_barrier();
  if (rank_me != root) {
    for (size_t chunk = 0; chunk < nchunks; ++chunk) {
      char key[LCT_PMI_STRING_LIMIT];
      char value[LCT_PMI_STRING_LIMIT];
      memset(key, 0, LCT_PMI_STRING_LIMIT);
      memset(value, 0, LCT_PMI_STRING_LIMIT);
      snprintf(key, LCT_PMI_STRING_LIMIT, "LCI_BOOTSTRAP_BCAST_%d_%d_%lu",
               round, root, chunk);
      LCT_pmi_getname(root, key, value);
      const size_t bytes = detail::chunk_size(size, chunk);
      detail::decode_value(value, bytes,
                           static_cast<char*>(recvbuf) +
                               chunk * detail::PMI_ENCODED_CHUNK_BYTES);
    }
  }
  LCT_pmi_barrier();
  LCI_Log(LOG_INFO, "bootstrap", "Bootstrap broadcast round %d done\n", round);
}

void barrier() { LCT_pmi_barrier(); }
}  // namespace bootstrap
}  // namespace lci
