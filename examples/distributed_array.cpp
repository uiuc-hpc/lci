// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <iostream>
#include <cassert>
#include "lci.hpp"

// This example shows the usage of the LCI one-sided RMA operations through the
// implementation of a simple distributed array.

template <typename T>
class distributed_array_t
{
 public:
  distributed_array_t(size_t size, T default_val) : m_size(size)
  {
    m_per_rank_size = size / lci::get_rank_n();
    m_local_start = lci::get_rank_me() * m_per_rank_size;
    m_data.resize(m_per_rank_size, default_val);
    m_rmrs.resize(lci::get_rank_n());
    // RMA operations allow users to directly read/write remote memory on other
    // processes. To enable this, the following steps are required: (1) Each
    // target process (i.e., the process whose memory will be accessed remotely)
    //     must register its local memory region and obtain a corresponding
    //     remote key (rmr).
    // (2) Each target process must then share its base address and rmr with
    // all other ranks.
    //     This is typically done using an allgather or similar collective
    //     operation.
    // These steps ensure that every process has the information needed to
    // perform one-sided RMA operations (e.g., put/get) to any other process's
    // registered memory buffer.
    mr = lci::register_memory(m_data.data(), m_data.size() * sizeof(T));
    // exchange the memory registration information with other ranks
    lci::rmr_t rmr = lci::get_rmr(mr);
    lci::allgather(&rmr, m_rmrs.data(), sizeof(lci::rmr_t));
  }

  ~distributed_array_t()
  {
    // deregister my memory buffer
    // we need a barrier to ensure that all remote operations are completed
    lci::barrier();
    lci::deregister_memory(&mr);
  }

  // a blocking get operation
  T get(size_t index)
  {
    int target_rank = get_target_rank(index);
    size_t local_index = get_local_index(index);
    lci::status_t status;
    T value;
    do {
      status = lci::post_get(target_rank, &value, sizeof(T),
                             lci::COMP_NULL_EXPECT_DONE_OR_RETRY,
                             local_index * sizeof(T), m_rmrs[target_rank]);
      lci::progress();
    } while (status.is_retry());
    assert(status.is_done());
    return value;
  }

  // a blocking put operation
  void put(size_t index, const T& value)
  {
    int target_rank = get_target_rank(index);
    size_t local_index = get_local_index(index);
    lci::status_t status;
    do {
      status = lci::post_put_x(target_rank,
                               static_cast<void*>(const_cast<int*>(&value)),
                               sizeof(T), lci::COMP_NULL_EXPECT_DONE_OR_RETRY,
                               local_index * sizeof(T), m_rmrs[target_rank])
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.is_retry());
    assert(status.is_done());
  }

 private:
  size_t m_size;
  size_t m_per_rank_size;
  size_t m_local_start;
  std::vector<T> m_data;
  // LCI memory registration information
  lci::mr_t mr;
  std::vector<lci::rmr_t> m_rmrs;

  int get_target_rank(size_t index) const
  {
    assert(index < m_size);
    return index / m_per_rank_size;
  }

  size_t get_local_index(size_t index) const
  {
    assert(index < m_size);
    return index % m_per_rank_size;
  }
};

void work(size_t size)
{
  distributed_array_t<int> darray(size, 0);
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();

  for (size_t i = rank; i < size; i += nranks) {
    darray.put(i, i);
  }
  lci::barrier();
  for (size_t i = (rank + 1) % nranks; i < size; i += nranks) {
    int value = darray.get(i);
    assert(value == i);
  }
}

int main(int argc, char** args)
{
  const size_t size = 1000;

  lci::g_runtime_init();
  work(size);
  lci::g_runtime_fina();
  return 0;
}