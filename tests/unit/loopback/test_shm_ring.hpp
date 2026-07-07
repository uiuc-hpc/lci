// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#if !defined(_WIN32)
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lci
{
namespace shm
{
// Narrow friend shim for deterministic protocol-state tests. Production sends
// use only ring_t::post_send().
class ring_test_access_t
{
 public:
  static error_t post_send_paused_after_reserve(
      ring_t& ring, int source_local_rank, const void* buffer, size_t size,
      net_imm_data_t imm_data, std::mutex* mutex,
      std::condition_variable* condition, bool* reserved, bool* proceed,
      bool* resumed_by_signal)
  {
    ring_t::send_reservation_t reservation;
    const error_t status = ring.reserve(&reservation);
    {
      std::lock_guard<std::mutex> lock(*mutex);
      *reserved = true;
    }
    condition->notify_all();
    if (!status.is_done()) return status;
    {
      std::unique_lock<std::mutex> lock(*mutex);
      *resumed_by_signal = condition->wait_for(lock, std::chrono::seconds(5),
                                               [&] { return *proceed; });
    }
    return ring.publish(&reservation, source_local_rank, buffer, size,
                        imm_data);
  }

  struct competing_send_t {
    ring_t* ring;
    error_t status = errorcode_t::fatal;
    uint64_t value = 0;
  };

  static error_t force_retry_lock(ring_t& ring, competing_send_t* competing)
  {
    ring_t::send_reservation_t reservation;
    const error_t status = ring.reserve(&reservation, run_competing_send,
                                        static_cast<void*>(competing));
    if (status.is_done()) {
      const uint64_t value = UINT64_C(0xfeedface);
      return ring.publish(&reservation, 1, &value, sizeof(value), 1);
    }
    return status;
  }

  static uint64_t encoded(uint64_t position, bool published)
  {
    return ring_t::encode_sequence(position,
                                   published ? ring_t::slot_state_t::published
                                             : ring_t::slot_state_t::reusable);
  }

  static bool initialize_at(void* region, size_t region_size, size_t slot_count,
                            size_t slot_size, uint64_t initial_position)
  {
    return ring_t::initialize_at(region, region_size, slot_count, slot_size,
                                 initial_position);
  }

  static void set_producer_position(ring_t& ring, uint64_t position)
  {
    ring.producer_position()->store(position, std::memory_order_relaxed);
  }

  static void set_consumer_position(ring_t& ring, uint64_t position)
  {
    ring.consumer_position()->store(position, std::memory_order_relaxed);
  }

  static void set_slot_sequence(ring_t& ring, uint64_t position,
                                uint64_t sequence)
  {
    auto* word = reinterpret_cast<std::atomic<uint64_t>*>(
        static_cast<void*>(ring.slot_at(position)));
    word->store(sequence, std::memory_order_relaxed);
  }

  static void corrupt_payload_size(ring_t& ring, uint64_t position)
  {
    struct header_prefix_t {
      std::atomic<uint64_t> sequence;
      uint32_t payload_size;
    };
    auto* header = reinterpret_cast<header_prefix_t*>(
        static_cast<void*>(ring.slot_at(position)));
    header->payload_size = static_cast<uint32_t>(ring.max_payload_size() + 1);
  }

 private:
  static void run_competing_send(void* argument)
  {
    auto* competing = static_cast<competing_send_t*>(argument);
    competing->status = competing->ring->post_send(
        9, &competing->value, sizeof(competing->value), 0x99);
  }
};
}  // namespace shm
}  // namespace lci

namespace test_shm_ring
{
using clock_t = std::chrono::steady_clock;

class local_ring_t
{
 public:
  local_ring_t(size_t slot_count, size_t slot_size, size_t cas_attempts = 8,
               uint64_t initial_position = 0)
      : size(lci::shm::ring_t::required_size(slot_count, slot_size))
  {
    EXPECT_NE(size, 0u);
    EXPECT_EQ(posix_memalign(&memory, LCI_CACHE_LINE, size), 0);
    EXPECT_TRUE(lci::shm::ring_test_access_t::initialize_at(
        memory, size, slot_count, slot_size, initial_position));
    ring.reset(new lci::shm::ring_t(memory, size, slot_count, slot_size,
                                    cas_attempts));
    EXPECT_TRUE(ring->is_valid());
  }

  ~local_ring_t()
  {
    ring.reset();
    free(memory);
  }

  void* memory = nullptr;
  size_t size = 0;
  std::unique_ptr<lci::shm::ring_t> ring;
};

TEST(SHM_RING, geometry_and_argument_validation)
{
  alignas(LCI_CACHE_LINE) unsigned char memory[4 * LCI_CACHE_LINE] = {};
  EXPECT_EQ(lci::shm::ring_t::required_size(0, 128), 0u);
  EXPECT_EQ(lci::shm::ring_t::required_size(3, 128), 0u);
  EXPECT_EQ(lci::shm::ring_t::required_size(4, 127), 0u);
  EXPECT_FALSE(lci::shm::ring_t::initialize(nullptr, sizeof(memory), 1, 128));
  EXPECT_FALSE(lci::shm::ring_test_access_t::initialize_at(
      memory, sizeof(memory), 1, 128,
      lci::shm::ring_t::position_mask + UINT64_C(1)));

  lci::shm::ring_t invalid(memory, sizeof(memory), 1, 128, 0);
  EXPECT_FALSE(invalid.is_valid());
  EXPECT_EQ(invalid.post_send(0, nullptr, 0, 0).errorcode,
            lci::errorcode_t::fatal);

  local_ring_t storage(1, 128);
  EXPECT_TRUE(storage.ring->is_consistent_empty());
  EXPECT_EQ(storage.ring->post_send(-1, nullptr, 0, 0).errorcode,
            lci::errorcode_t::fatal);
  EXPECT_EQ(storage.ring
                ->post_send(0, nullptr, storage.ring->max_payload_size() + 1, 0)
                .errorcode,
            lci::errorcode_t::fatal);
}

TEST(SHM_RING, capacity_one_does_not_alias_published_and_free_states)
{
  local_ring_t storage(1, 128);
  auto& ring = *storage.ring;
  const uint64_t first = 11;
  const uint64_t second = 22;
  ASSERT_TRUE(ring.post_send(3, &first, sizeof(first), 0x11).is_done());
  EXPECT_EQ(ring.post_send(4, &second, sizeof(second), 0x22).errorcode,
            lci::errorcode_t::retry_nomem);

  lci::shm::recv_slot_t view;
  ASSERT_TRUE(ring.poll(&view));
  EXPECT_EQ(view.source_local_rank, 3);
  EXPECT_EQ(view.imm_data, 0x11u);
  ASSERT_EQ(view.size, sizeof(first));
  EXPECT_EQ(*static_cast<const uint64_t*>(view.payload), first);
  EXPECT_EQ(ring.post_send(4, &second, sizeof(second), 0x22).errorcode,
            lci::errorcode_t::retry_nomem);
  ASSERT_TRUE(ring.release(&view));

  ASSERT_TRUE(ring.post_send(4, &second, sizeof(second), 0x22).is_done());
  ASSERT_TRUE(ring.poll(&view));
  EXPECT_EQ(view.source_local_rank, 4);
  EXPECT_EQ(*static_cast<const uint64_t*>(view.payload), second);
  EXPECT_TRUE(ring.release(&view));
}

TEST(SHM_RING, zero_max_payload_source_copy_and_multiple_outstanding)
{
  local_ring_t storage(2, 128);
  auto& ring = *storage.ring;
  lci::shm::recv_slot_t first;
  lci::shm::recv_slot_t second;
  EXPECT_FALSE(ring.poll(&first));

  ASSERT_TRUE(ring.post_send(3, nullptr, 0, 0x1234).is_done());
  std::vector<unsigned char> source(ring.max_payload_size(), 0x5a);
  ASSERT_TRUE(
      ring.post_send(7, source.data(), source.size(), 0xabcdef).is_done());
  std::fill(source.begin(), source.end(), 0);
  ASSERT_TRUE(ring.poll(&first));
  ASSERT_TRUE(ring.poll(&second));
  EXPECT_EQ(first.source_local_rank, 3);
  EXPECT_EQ(first.size, 0u);
  EXPECT_EQ(second.source_local_rank, 7);
  EXPECT_EQ(second.imm_data, 0xabcdefu);
  ASSERT_EQ(second.size, ring.max_payload_size());
  const auto* payload = static_cast<const unsigned char*>(second.payload);
  EXPECT_TRUE(std::all_of(payload, payload + second.size,
                          [](unsigned char value) { return value == 0x5a; }));

  EXPECT_TRUE(ring.release(&second));
  EXPECT_EQ(ring.post_send(8, source.data(), 1, 0).errorcode,
            lci::errorcode_t::retry_nomem);
  EXPECT_TRUE(ring.release(&first));
  EXPECT_TRUE(ring.post_send(8, source.data(), 1, 0).is_done());
  ASSERT_TRUE(ring.poll(&first));
  EXPECT_TRUE(ring.release(&first));
}

TEST(SHM_RING, counter_wrap_has_defined_power_of_two_geometry)
{
  const uint64_t initial = lci::shm::ring_t::position_mask - 1;
  local_ring_t storage(1, 128, 8, initial);
  auto& ring = *storage.ring;
  lci::shm::recv_slot_t view;
  const uint64_t values[] = {initial, initial + 1, 0, 1};
  for (uint64_t value : values) {
    ASSERT_TRUE(ring.post_send(0, &value, sizeof(value), value).is_done());
    ASSERT_TRUE(ring.poll(&view));
    EXPECT_EQ(*static_cast<const uint64_t*>(view.payload), value);
    EXPECT_EQ(view.imm_data, static_cast<lci::net_imm_data_t>(value));
    ASSERT_TRUE(ring.release(&view));
  }

  local_ring_t four_slots(4, 128, 8, lci::shm::ring_t::position_mask - 2);
  auto& wrap_ring = *four_slots.ring;
  for (uint64_t value = 0; value < 4; ++value) {
    ASSERT_TRUE(wrap_ring.post_send(1, &value, sizeof(value), value).is_done());
  }
  for (uint64_t value = 0; value < 4; ++value) {
    ASSERT_TRUE(wrap_ring.poll(&view));
    EXPECT_EQ(*static_cast<const uint64_t*>(view.payload), value);
    ASSERT_TRUE(wrap_ring.release(&view));
  }
}

TEST(SHM_RING, delayed_producer_blocks_fifo_but_not_later_publication)
{
  local_ring_t storage(2, 128);
  auto& ring = *storage.ring;
  const uint64_t first = 10;
  const uint64_t second = 20;
  std::mutex mutex;
  std::condition_variable condition;
  bool reserved = false;
  bool proceed = false;
  bool resumed_by_signal = false;
  lci::error_t first_status = lci::errorcode_t::fatal;
  std::thread producer([&] {
    first_status = lci::shm::ring_test_access_t::post_send_paused_after_reserve(
        ring, 1, &first, sizeof(first), 10, &mutex, &condition, &reserved,
        &proceed, &resumed_by_signal);
  });

  bool reservation_observed = false;
  {
    std::unique_lock<std::mutex> lock(mutex);
    reservation_observed = condition.wait_for(lock, std::chrono::seconds(2),
                                              [&] { return reserved; });
  }
  const lci::error_t second_status =
      ring.post_send(2, &second, sizeof(second), 20);
  lci::shm::recv_slot_t view;
  const bool polled_before_publication = ring.poll(&view);
  {
    std::lock_guard<std::mutex> lock(mutex);
    proceed = true;
  }
  condition.notify_all();
  producer.join();

  ASSERT_TRUE(reservation_observed);
  ASSERT_TRUE(resumed_by_signal);
  ASSERT_TRUE(first_status.is_done());
  ASSERT_TRUE(second_status.is_done());
  EXPECT_FALSE(polled_before_publication);

  ASSERT_TRUE(ring.poll(&view));
  EXPECT_EQ(*static_cast<const uint64_t*>(view.payload), first);
  ASSERT_TRUE(ring.release(&view));
  ASSERT_TRUE(ring.poll(&view));
  EXPECT_EQ(*static_cast<const uint64_t*>(view.payload), second);
  ASSERT_TRUE(ring.release(&view));
}

TEST(SHM_RING, bounded_cas_conflict_returns_retry_lock)
{
  local_ring_t storage(2, 128, 1);
  auto& ring = *storage.ring;
  lci::shm::ring_test_access_t::competing_send_t competing = {&ring};
  competing.value = 1234;
  const lci::error_t status =
      lci::shm::ring_test_access_t::force_retry_lock(ring, &competing);
  EXPECT_EQ(status.errorcode, lci::errorcode_t::retry_lock);
  ASSERT_TRUE(competing.status.is_done());
  lci::shm::recv_slot_t view;
  ASSERT_TRUE(ring.poll(&view));
  EXPECT_EQ(*static_cast<const uint64_t*>(view.payload), competing.value);
  EXPECT_TRUE(ring.release(&view));
}

TEST(SHM_RING, malformed_control_sequence_and_metadata_are_not_empty_or_full)
{
  {
    local_ring_t storage(1, 128);
    auto& ring = *storage.ring;
    lci::shm::ring_test_access_t::set_producer_position(
        ring, lci::shm::ring_t::position_modulus);
    EXPECT_EQ(ring.post_send(0, nullptr, 0, 0).errorcode,
              lci::errorcode_t::fatal);
  }
  {
    local_ring_t storage(1, 128);
    auto& ring = *storage.ring;
    lci::shm::ring_test_access_t::set_consumer_position(
        ring, lci::shm::ring_t::position_modulus);
    lci::shm::recv_slot_t view;
    EXPECT_THROW(ring.poll(&view), std::runtime_error);
  }
  {
    local_ring_t storage(1, 128);
    auto& ring = *storage.ring;
    lci::shm::ring_test_access_t::set_slot_sequence(
        ring, 0, lci::shm::ring_test_access_t::encoded(17, true));
    EXPECT_EQ(ring.post_send(0, nullptr, 0, 0).errorcode,
              lci::errorcode_t::fatal);
    lci::shm::recv_slot_t view;
    EXPECT_THROW(ring.poll(&view), std::runtime_error);
  }
  {
    local_ring_t storage(1, 128);
    auto& ring = *storage.ring;
    ASSERT_TRUE(ring.post_send(0, nullptr, 0, 0).is_done());
    lci::shm::ring_test_access_t::corrupt_payload_size(ring, 0);
    lci::shm::recv_slot_t view;
    EXPECT_THROW(ring.poll(&view), std::runtime_error);
    EXPECT_TRUE(ring.post_send(0, nullptr, 0, 0).is_done());
    ASSERT_TRUE(ring.poll(&view));
    EXPECT_TRUE(ring.release(&view));
  }
  {
    local_ring_t storage(1, 128);
    auto& ring = *storage.ring;
    ASSERT_TRUE(ring.post_send(0, nullptr, 0, 0).is_done());
    lci::shm::recv_slot_t view;
    ASSERT_TRUE(ring.poll(&view));
    lci::shm::ring_test_access_t::set_slot_sequence(
        ring, 0, lci::shm::ring_test_access_t::encoded(19, true));
    EXPECT_THROW(ring.release(&view), std::runtime_error);
  }
}

TEST(SHM_RING, ring_identity_rejects_stale_release_after_reconstruction)
{
  static_assert(!std::is_copy_constructible<lci::shm::ring_t>::value, "");
  static_assert(!std::is_move_constructible<lci::shm::ring_t>::value, "");
  const size_t size = lci::shm::ring_t::required_size(1, 128);
  void* memory = nullptr;
  ASSERT_EQ(posix_memalign(&memory, LCI_CACHE_LINE, size), 0);
  ASSERT_TRUE(lci::shm::ring_t::initialize(memory, size, 1, 128));
  alignas(
      lci::shm::ring_t) unsigned char ring_storage[sizeof(lci::shm::ring_t)];
  auto* first = new (ring_storage) lci::shm::ring_t(memory, size, 1, 128);
  ASSERT_TRUE(first->post_send(0, nullptr, 0, 0).is_done());
  lci::shm::recv_slot_t view;
  ASSERT_TRUE(first->poll(&view));
  first->~ring_t();
  auto* replacement = new (ring_storage) lci::shm::ring_t(memory, size, 1, 128);
  EXPECT_FALSE(replacement->release(&view));
  replacement->~ring_t();
  free(memory);
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

TEST(SHM_RING, bounded_multithreaded_mpmc_visibility_and_uniqueness)
{
  constexpr uint32_t nproducers = 8;
  constexpr uint32_t nconsumers = 4;
  constexpr uint32_t nmessages = 5000;
  constexpr uint32_t total = nproducers * nmessages;
  local_ring_t storage(256, 128, 4);
  auto& ring = *storage.ring;
  ASSERT_GE(ring.max_payload_size(), sizeof(stress_message_t));
  const auto deadline = clock_t::now() + std::chrono::seconds(15);

  std::unique_ptr<std::atomic<unsigned char>[]> seen(
      new std::atomic<unsigned char>[total]);
  for (uint32_t i = 0; i < total; ++i) seen[i].store(0);
  std::atomic<uint32_t> consumed(0);
  std::atomic<uint32_t> errors(0);
  std::atomic<bool> stop(false);
  std::vector<std::thread> threads;

  for (uint32_t producer = 0; producer < nproducers; ++producer) {
    threads.emplace_back([&, producer] {
      for (uint32_t sequence = 0;
           sequence < nmessages && !stop.load(std::memory_order_relaxed);
           ++sequence) {
        const auto message = make_message(producer, sequence);
        while (!stop.load(std::memory_order_relaxed)) {
          const auto status =
              ring.post_send(producer, &message, sizeof(message), sequence);
          if (status.is_done()) break;
          if (!status.is_retry() || clock_t::now() >= deadline) {
            errors.fetch_add(1);
            stop.store(true);
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
      while (!stop.load(std::memory_order_relaxed) &&
             consumed.load(std::memory_order_relaxed) < total) {
        if (!ring.poll(&view)) {
          if (clock_t::now() >= deadline) {
            errors.fetch_add(1);
            stop.store(true);
          } else {
            std::this_thread::yield();
          }
          continue;
        }
        stress_message_t message = {};
        if (view.size == sizeof(message)) {
          std::memcpy(&message, view.payload, sizeof(message));
        }
        if (view.size != sizeof(message) ||
            !valid_message(message, nproducers, nmessages) ||
            view.source_local_rank != static_cast<int>(message.producer) ||
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
  for (auto& thread : threads) thread.join();

  EXPECT_FALSE(stop.load());
  EXPECT_EQ(consumed.load(), total);
  EXPECT_EQ(errors.load(), 0u);
  if (consumed.load() == total) {
    for (uint32_t i = 0; i < total; ++i) EXPECT_EQ(seen[i].load(), 1);
  }
}

#if !defined(_WIN32)
class child_guard_t
{
 public:
  ~child_guard_t() { terminate_and_wait(); }

  void add(pid_t child) { children.push_back(child); }

  bool wait_until(clock_t::time_point deadline)
  {
    bool success = true;
    for (pid_t& child : children) {
      if (child <= 0) continue;
      int status = 0;
      while (clock_t::now() < deadline) {
        const pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
          child = -1;
          success = success && WIFEXITED(status) && WEXITSTATUS(status) == 0;
          break;
        }
        if (result < 0 && errno != EINTR) {
          child = -1;
          success = false;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      if (child > 0) success = false;
    }
    if (!success) terminate_and_wait();
    return success;
  }

 private:
  void terminate_and_wait()
  {
    for (pid_t& child : children) {
      if (child <= 0) continue;
      kill(child, SIGKILL);
      int status = 0;
      while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
      }
      child = -1;
    }
  }

  std::vector<pid_t> children;
};

TEST(SHM_RING, bounded_eight_process_four_consumer_stress)
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
  const auto deadline = clock_t::now() + std::chrono::seconds(15);
  child_guard_t children;
  bool fork_failed = false;

  for (uint32_t producer = 0; producer < nproducers; ++producer) {
    const pid_t child = fork();
    if (child < 0) {
      fork_failed = true;
      break;
    }
    if (child == 0) {
      for (uint32_t sequence = 0; sequence < nmessages; ++sequence) {
        const auto message = make_message(producer, sequence);
        while (true) {
          const auto status =
              ring.post_send(producer, &message, sizeof(message), sequence);
          if (status.is_done()) break;
          if (!status.is_retry()) _exit(2);
          if (clock_t::now() >= deadline) _exit(3);
          std::this_thread::yield();
        }
      }
      _exit(0);
    }
    children.add(child);
  }

  std::unique_ptr<std::atomic<unsigned char>[]> seen(
      new std::atomic<unsigned char>[total]);
  for (uint32_t i = 0; i < total; ++i) seen[i].store(0);
  std::atomic<uint32_t> consumed(0);
  std::atomic<uint32_t> errors(fork_failed ? 1 : 0);
  std::atomic<bool> stop(fork_failed);
  std::vector<std::thread> consumers;
  for (uint32_t consumer = 0; consumer < nconsumers; ++consumer) {
    consumers.emplace_back([&] {
      lci::shm::recv_slot_t view;
      while (!stop.load(std::memory_order_relaxed) &&
             consumed.load(std::memory_order_relaxed) < total) {
        if (!ring.poll(&view)) {
          if (clock_t::now() >= deadline) {
            errors.fetch_add(1);
            stop.store(true);
          } else {
            std::this_thread::yield();
          }
          continue;
        }
        stress_message_t message = {};
        if (view.size == sizeof(message)) {
          std::memcpy(&message, view.payload, sizeof(message));
        }
        if (view.size != sizeof(message) ||
            !valid_message(message, nproducers, nmessages) ||
            view.source_local_rank != static_cast<int>(message.producer) ||
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
  const bool children_ok = children.wait_until(deadline);

  EXPECT_FALSE(stop.load());
  EXPECT_TRUE(children_ok);
  EXPECT_EQ(consumed.load(), total);
  EXPECT_EQ(errors.load(), 0u);
  if (consumed.load() == total) {
    for (uint32_t i = 0; i < total; ++i) EXPECT_EQ(seen[i].load(), 1);
  }
  EXPECT_EQ(munmap(memory, region_size), 0);
}

static std::string unique_shm_name(const char* suffix, uint32_t iteration = 0)
{
  return std::string("/lci-test-") + std::to_string(getpid()) + "-" + suffix +
         "-" + std::to_string(iteration);
}

static lci::shm::posix_ring_expected_t expected_ring(
    const std::string& name, int owner_global_rank, uint64_t device_uid,
    size_t slot_count, size_t slot_size, size_t max_message_size)
{
  lci::shm::posix_ring_expected_t expected;
  expected.owner_global_rank = owner_global_rank;
  expected.device_uid = device_uid;
  expected.mapping_size =
      lci::shm::posix_ring_mapping_size(slot_count, slot_size);
  expected.slot_count = slot_count;
  expected.slot_size = slot_size;
  expected.max_message_size = max_message_size;
  expected.name = name;
  return expected;
}

TEST(SHM_POSIX_RING, repeated_owner_attach_unlink_and_message_lifecycle)
{
  for (uint32_t iteration = 0; iteration < 20; ++iteration) {
    const std::string name = unique_shm_name("lifecycle", iteration);
    const auto expected =
        expected_ring(name, 2, UINT64_C(0x12345678), 1, 128, 64);
    std::string error;
    auto owner = lci::shm::posix_owner_mapping_t::create(
        name, 2, UINT64_C(0x12345678), 1, 128, 64, 4, &error);
    ASSERT_NE(owner, nullptr) << error;
    auto peer = lci::shm::posix_peer_mapping_t::attach(owner->handle(),
                                                       expected, 4, &error);
    ASSERT_NE(peer, nullptr) << error;
    EXPECT_TRUE(owner->unlink_name(&error)) << error;
    EXPECT_TRUE(owner->unlink_name(&error)) << error;
    EXPECT_EQ(lci::shm::posix_peer_mapping_t::attach(owner->handle(), expected,
                                                     4, &error),
              nullptr);

    const uint64_t value = iteration;
    ASSERT_TRUE(
        peer->ring().post_send(5, &value, sizeof(value), iteration).is_done());
    lci::shm::recv_slot_t view;
    ASSERT_TRUE(owner->ring().poll(&view));
    EXPECT_EQ(view.source_local_rank, 5);
    EXPECT_EQ(*static_cast<const uint64_t*>(view.payload), value);
    EXPECT_TRUE(owner->ring().release(&view));
  }
}

TEST(SHM_POSIX_RING, generated_object_names_are_compact_and_identity_based)
{
  const std::array<uint64_t, 2> nonce = {
      {UINT64_C(0x0123456789abcdef), UINT64_C(0xfedcba9876543210)}};
  const uint64_t device_uid = UINT64_C(0x1020304050607080);

  const std::string name =
      lci::shm::make_posix_ring_name(nonce, device_uid, INT32_MAX);
  EXPECT_LE(name.size(), lci::shm::posix_ring_portable_name_max);
  EXPECT_EQ(name.find('/', 1), std::string::npos);

  std::string error;
  auto owner = lci::shm::posix_owner_mapping_t::create(
      name, INT32_MAX, device_uid, 1, 128, 64, 2, &error);
  ASSERT_NE(owner, nullptr) << error;

  auto expected = expected_ring(name, INT32_MAX, device_uid, 1, 128, 64);
  EXPECT_TRUE(
      lci::shm::validate_posix_ring_handle(owner->handle(), expected, &error))
      << error;

  auto changed_nonce = nonce;
  changed_nonce[1] ^= UINT64_C(1);
  EXPECT_NE(name, lci::shm::make_posix_ring_name(changed_nonce, device_uid,
                                                 INT32_MAX));
  EXPECT_NE(name,
            lci::shm::make_posix_ring_name(nonce, device_uid + 1, INT32_MAX));
  EXPECT_NE(name,
            lci::shm::make_posix_ring_name(nonce, device_uid, INT32_MAX - 1));
}

TEST(SHM_POSIX_RING, rejects_malformed_handles_and_cleans_failure_paths)
{
  const std::string name = unique_shm_name("failure");
  std::string error;
  auto owner = lci::shm::posix_owner_mapping_t::create(name, 1, 99, 2, 128, 64,
                                                       2, &error);
  ASSERT_NE(owner, nullptr) << error;
  EXPECT_EQ(lci::shm::posix_owner_mapping_t::create(name, 1, 100, 2, 128, 64, 2,
                                                    &error),
            nullptr);
  const auto expected = expected_ring(name, 1, 99, 2, 128, 64);

  auto malformed = owner->handle();
  malformed.version++;
  EXPECT_FALSE(lci::shm::validate_posix_ring_handle(malformed, &error));
  malformed = owner->handle();
  malformed.mapping_size++;
  EXPECT_FALSE(lci::shm::validate_posix_ring_handle(malformed, &error));
  malformed = owner->handle();
  malformed.slot_count = 3;
  EXPECT_FALSE(lci::shm::validate_posix_ring_handle(malformed, &error));
  malformed = owner->handle();
  std::memset(malformed.name, 'x', sizeof(malformed.name));
  EXPECT_FALSE(lci::shm::validate_posix_ring_handle(malformed, &error));

  lci::shm::ring_test_access_t::set_slot_sequence(
      owner->ring(), 0, lci::shm::ring_test_access_t::encoded(17, true));
  EXPECT_EQ(lci::shm::posix_peer_mapping_t::attach(owner->handle(), expected, 2,
                                                   &error),
            nullptr);

  const auto handle = owner->handle();
  owner.reset();
  errno = 0;
  const int fd = shm_open(name.c_str(), O_RDWR, 0600);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENOENT);
  if (fd >= 0) close(fd);

  // Attach must reject an object at the expected name with the wrong backing
  // size. Use a fresh undersized object instead of resizing the live owner
  // mapping; not all POSIX shm implementations support live resizing.
  const int undersized_fd =
      shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
  ASSERT_GE(undersized_fd, 0);
  ASSERT_EQ(close(undersized_fd), 0);
  EXPECT_EQ(lci::shm::posix_peer_mapping_t::attach(handle, expected, 2, &error),
            nullptr);
  EXPECT_EQ(shm_unlink(name.c_str()), 0);

  const std::string bad_name = unique_shm_name("bad-geometry");
  EXPECT_EQ(lci::shm::posix_owner_mapping_t::create(bad_name, 1, 1, 3, 128, 64,
                                                    1, &error),
            nullptr);
  errno = 0;
  const int bad_fd = shm_open(bad_name.c_str(), O_RDWR, 0600);
  EXPECT_EQ(bad_fd, -1);
  EXPECT_EQ(errno, ENOENT);
  if (bad_fd >= 0) close(bad_fd);
}
TEST(SHM_POSIX_RING,
     attachment_requires_authoritative_identity_name_and_exact_geometry)
{
  const std::string name = unique_shm_name("authoritative");
  std::string error;
  auto owner = lci::shm::posix_owner_mapping_t::create(
      name, 7, UINT64_C(0x778899), 2, 128, 64, 2, &error);
  ASSERT_NE(owner, nullptr) << error;
  const auto expected = expected_ring(name, 7, UINT64_C(0x778899), 2, 128, 64);

  auto peer = lci::shm::posix_peer_mapping_t::attach(owner->handle(), expected,
                                                     2, &error);
  ASSERT_NE(peer, nullptr) << error;
  peer.reset();

  auto received = owner->handle();
  received.owner_global_rank = 8;
  EXPECT_EQ(
      lci::shm::posix_peer_mapping_t::attach(received, expected, 2, &error),
      nullptr);
  received = owner->handle();
  received.device_uid++;
  EXPECT_EQ(
      lci::shm::posix_peer_mapping_t::attach(received, expected, 2, &error),
      nullptr);
  received = owner->handle();
  std::memset(received.name, 0, sizeof(received.name));
  const std::string other_name = unique_shm_name("wrong-name");
  std::memcpy(received.name, other_name.c_str(), other_name.size() + 1);
  EXPECT_EQ(
      lci::shm::posix_peer_mapping_t::attach(received, expected, 2, &error),
      nullptr);
  received = owner->handle();
  received.max_message_size = 32;
  EXPECT_EQ(
      lci::shm::posix_peer_mapping_t::attach(received, expected, 2, &error),
      nullptr);

  // These geometries have the same total mapping size. Exact out-of-band
  // comparison must reject the alias before it can be interpreted as a valid
  // one-slot ring.
  static_assert(lci::shm::ring_t::control_size + 2 * 128 ==
                    lci::shm::ring_t::control_size + 1 * 256,
                "test geometries must alias by total size");
  received = owner->handle();
  received.slot_count = 1;
  received.slot_size = 256;
  EXPECT_TRUE(lci::shm::validate_posix_ring_handle(received, &error)) << error;
  EXPECT_EQ(
      lci::shm::posix_peer_mapping_t::attach(received, expected, 2, &error),
      nullptr);
}
#endif

}  // namespace test_shm_ring
