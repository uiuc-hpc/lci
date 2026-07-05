// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace test_shm_ring
{
class local_ring_t
{
 public:
  local_ring_t(size_t slot_count, size_t slot_size, size_t cas_attempts = 8)
      : size(lci::shm::ring_t::required_size(slot_count, slot_size))
  {
    EXPECT_NE(size, 0u);
    EXPECT_EQ(posix_memalign(&memory, LCI_CACHE_LINE, size), 0);
    EXPECT_TRUE(
        lci::shm::ring_t::initialize(memory, size, slot_count, slot_size));
    ring.reset(new lci::shm::ring_t(memory, size, slot_count, slot_size,
                                    cas_attempts));
    EXPECT_TRUE(ring->is_valid());
  }

  ~local_ring_t() { free(memory); }

  void* memory = nullptr;
  size_t size = 0;
  std::unique_ptr<lci::shm::ring_t> ring;
};

TEST(SHM_RING, validation)
{
  alignas(LCI_CACHE_LINE) unsigned char memory[4 * LCI_CACHE_LINE] = {};
  EXPECT_EQ(lci::shm::ring_t::required_size(0, 128), 0u);
  EXPECT_EQ(lci::shm::ring_t::required_size(4, 127), 0u);
  EXPECT_FALSE(lci::shm::ring_t::initialize(nullptr, sizeof(memory), 1, 128));

  lci::shm::ring_t invalid(memory, sizeof(memory), 1, 128, 0);
  EXPECT_FALSE(invalid.is_valid());
  EXPECT_EQ(invalid.post_send(0, nullptr, 0, 0).errorcode,
            lci::errorcode_t::fatal);
}

TEST(SHM_RING, empty_zero_max_and_source_copy)
{
  local_ring_t storage(4, 128);
  auto& ring = *storage.ring;
  lci::shm::recv_slot_t view;
  EXPECT_FALSE(ring.poll(&view));

  EXPECT_EQ(ring.post_send(3, nullptr, 0, 0x1234).errorcode,
            lci::errorcode_t::done);
  ASSERT_TRUE(ring.poll(&view));
  EXPECT_EQ(view.rank, 3);
  EXPECT_EQ(view.imm_data, 0x1234u);
  EXPECT_EQ(view.size, 0u);
  EXPECT_TRUE(ring.release(&view));

  std::vector<unsigned char> source(ring.max_payload_size(), 0x5a);
  ASSERT_FALSE(source.empty());
  EXPECT_EQ(ring.post_send(7, source.data(), source.size(), 0xabcdef).errorcode,
            lci::errorcode_t::done);
  std::fill(source.begin(), source.end(), 0);
  ASSERT_TRUE(ring.poll(&view));
  EXPECT_EQ(view.rank, 7);
  EXPECT_EQ(view.imm_data, 0xabcdefu);
  ASSERT_EQ(view.size, ring.max_payload_size());
  const auto* payload = static_cast<const unsigned char*>(view.payload);
  EXPECT_TRUE(std::all_of(payload, payload + view.size,
                          [](unsigned char value) { return value == 0x5a; }));
  EXPECT_TRUE(ring.release(&view));
  EXPECT_EQ(ring.post_send(0, source.data(), source.size() + 1, 0).errorcode,
            lci::errorcode_t::fatal);
}

TEST(SHM_RING, full_multiple_outstanding_and_wrap_recovery)
{
  local_ring_t storage(2, 128);
  auto& ring = *storage.ring;
  uint64_t values[] = {10, 20, 30};

  EXPECT_TRUE(ring.post_send(0, &values[0], sizeof(uint64_t), 10).is_done());
  EXPECT_TRUE(ring.post_send(1, &values[1], sizeof(uint64_t), 20).is_done());
  EXPECT_EQ(ring.post_send(2, &values[2], sizeof(uint64_t), 30).errorcode,
            lci::errorcode_t::retry_nomem);

  lci::shm::recv_slot_t first;
  lci::shm::recv_slot_t second;
  ASSERT_TRUE(ring.poll(&first));
  ASSERT_TRUE(ring.poll(&second));
  EXPECT_EQ(*static_cast<const uint64_t*>(first.payload), 10u);
  EXPECT_EQ(*static_cast<const uint64_t*>(second.payload), 20u);

  // Releasing a later claimed slot does not let wrap reclaim an earlier slot.
  EXPECT_TRUE(ring.release(&second));
  EXPECT_EQ(ring.post_send(2, &values[2], sizeof(uint64_t), 30).errorcode,
            lci::errorcode_t::retry_nomem);
  EXPECT_TRUE(ring.release(&first));
  EXPECT_TRUE(ring.post_send(2, &values[2], sizeof(uint64_t), 30).is_done());
  ASSERT_TRUE(ring.poll(&first));
  EXPECT_EQ(first.rank, 2);
  EXPECT_EQ(first.imm_data, 30u);
  EXPECT_EQ(*static_cast<const uint64_t*>(first.payload), 30u);
  EXPECT_TRUE(ring.release(&first));

  // Exercise many physical-slot generations and their sequence transitions.
  for (uint64_t i = 0; i < 10000; ++i) {
    ASSERT_TRUE(ring.post_send(0, &i, sizeof(i), 0).is_done());
    ASSERT_TRUE(ring.poll(&first));
    ASSERT_EQ(*static_cast<const uint64_t*>(first.payload), i);
    ASSERT_TRUE(ring.release(&first));
  }
}

struct stress_message_t {
  uint32_t producer;
  uint32_t sequence;
  uint64_t checksum;
  unsigned char fill[32];
};

static stress_message_t make_message(uint32_t producer, uint32_t sequence)
{
  stress_message_t message = {};
  message.producer = producer;
  message.sequence = sequence;
  message.checksum = (static_cast<uint64_t>(producer) << 32) ^ sequence ^
                     UINT64_C(0xd6e8feb86659fd93);
  std::memset(message.fill, static_cast<unsigned char>(producer ^ sequence),
              sizeof(message.fill));
  return message;
}

static bool valid_message(const stress_message_t& message, uint32_t producers,
                          uint32_t messages_per_producer)
{
  if (message.producer >= producers ||
      message.sequence >= messages_per_producer ||
      message.checksum != ((static_cast<uint64_t>(message.producer) << 32) ^
                           message.sequence ^ UINT64_C(0xd6e8feb86659fd93))) {
    return false;
  }
  const unsigned char expected =
      static_cast<unsigned char>(message.producer ^ message.sequence);
  return std::all_of(
      std::begin(message.fill), std::end(message.fill),
      [expected](unsigned char value) { return value == expected; });
}

TEST(SHM_RING, multithreaded_mpmc_visibility_and_uniqueness)
{
  constexpr uint32_t nproducers = 8;
  constexpr uint32_t nconsumers = 4;
  constexpr uint32_t nmessages = 5000;
  constexpr uint32_t total = nproducers * nmessages;
  local_ring_t storage(256, 128, 4);
  auto& ring = *storage.ring;
  ASSERT_GE(ring.max_payload_size(), sizeof(stress_message_t));

  std::unique_ptr<std::atomic<unsigned char>[]> seen(
      new std::atomic<unsigned char>[total]);
  for (uint32_t i = 0; i < total; ++i) seen[i].store(0);
  std::atomic<uint32_t> consumed(0);
  std::atomic<uint32_t> errors(0);
  std::vector<std::thread> threads;

  for (uint32_t producer = 0; producer < nproducers; ++producer) {
    threads.emplace_back([&, producer] {
      for (uint32_t sequence = 0; sequence < nmessages; ++sequence) {
        const auto message = make_message(producer, sequence);
        while (true) {
          auto status =
              ring.post_send(producer, &message, sizeof(message), sequence);
          if (status.is_done()) break;
          if (!status.is_retry()) {
            errors.fetch_add(1);
            break;
          }
          std::this_thread::yield();
        }
      }
    });
  }
  for (uint32_t consumer = 0; consumer < nconsumers; ++consumer) {
    threads.emplace_back([&] {
      lci::shm::recv_slot_t view;
      while (consumed.load(std::memory_order_relaxed) < total) {
        if (!ring.poll(&view)) {
          std::this_thread::yield();
          continue;
        }
        if (view.size != sizeof(stress_message_t)) {
          errors.fetch_add(1);
        } else {
          stress_message_t message;
          std::memcpy(&message, view.payload, sizeof(message));
          if (!valid_message(message, nproducers, nmessages) ||
              view.rank != static_cast<int>(message.producer) ||
              view.imm_data != message.sequence) {
            errors.fetch_add(1);
          } else {
            const uint32_t id = message.producer * nmessages + message.sequence;
            if (seen[id].fetch_add(1) != 0) errors.fetch_add(1);
          }
        }
        if (!ring.release(&view)) errors.fetch_add(1);
        consumed.fetch_add(1);
      }
    });
  }
  for (auto& thread : threads) thread.join();

  EXPECT_EQ(consumed.load(), total);
  EXPECT_EQ(errors.load(), 0u);
  for (uint32_t i = 0; i < total; ++i) EXPECT_EQ(seen[i].load(), 1);
}

#if !defined(_WIN32)
TEST(SHM_RING, eight_process_producers_four_consumer_threads)
{
  constexpr uint32_t nproducers = 8;
  constexpr uint32_t nconsumers = 4;
  constexpr uint32_t nmessages = 1000;
  constexpr uint32_t total = nproducers * nmessages;
  constexpr size_t slot_count = 256;
  constexpr size_t slot_size = 128;
  const size_t region_size =
      lci::shm::ring_t::required_size(slot_count, slot_size);
  void* memory = mmap(nullptr, region_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(memory, MAP_FAILED);
  ASSERT_TRUE(
      lci::shm::ring_t::initialize(memory, region_size, slot_count, slot_size));
  lci::shm::ring_t ring(memory, region_size, slot_count, slot_size, 4);
  ASSERT_TRUE(ring.is_valid());

  std::vector<pid_t> children;
  for (uint32_t producer = 0; producer < nproducers; ++producer) {
    const pid_t child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
      for (uint32_t sequence = 0; sequence < nmessages; ++sequence) {
        const auto message = make_message(producer, sequence);
        while (true) {
          const auto status =
              ring.post_send(producer, &message, sizeof(message), sequence);
          if (status.is_done()) break;
          if (!status.is_retry()) _exit(2);
          std::this_thread::yield();
        }
      }
      _exit(0);
    }
    children.push_back(child);
  }

  std::unique_ptr<std::atomic<unsigned char>[]> seen(
      new std::atomic<unsigned char>[total]);
  for (uint32_t i = 0; i < total; ++i) seen[i].store(0);
  std::atomic<uint32_t> consumed(0);
  std::atomic<uint32_t> errors(0);
  std::vector<std::thread> consumers;
  for (uint32_t consumer = 0; consumer < nconsumers; ++consumer) {
    consumers.emplace_back([&] {
      lci::shm::recv_slot_t view;
      while (consumed.load(std::memory_order_relaxed) < total) {
        if (!ring.poll(&view)) {
          std::this_thread::yield();
          continue;
        }
        stress_message_t message = {};
        if (view.size == sizeof(message)) {
          std::memcpy(&message, view.payload, sizeof(message));
        }
        if (view.size != sizeof(message) ||
            !valid_message(message, nproducers, nmessages) ||
            view.rank != static_cast<int>(message.producer) ||
            view.imm_data != message.sequence) {
          errors.fetch_add(1);
        } else {
          const uint32_t id = message.producer * nmessages + message.sequence;
          if (seen[id].fetch_add(1) != 0) errors.fetch_add(1);
        }
        if (!ring.release(&view)) errors.fetch_add(1);
        consumed.fetch_add(1);
      }
    });
  }
  for (auto& consumer : consumers) consumer.join();
  for (pid_t child : children) {
    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
  }

  EXPECT_EQ(consumed.load(), total);
  EXPECT_EQ(errors.load(), 0u);
  for (uint32_t i = 0; i < total; ++i) EXPECT_EQ(seen[i].load(), 1);
  EXPECT_EQ(munmap(memory, region_size), 0);
}
#endif

}  // namespace test_shm_ring
