// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#ifndef LCI_MATCHING_ENGINE_QUEUE_HPP
#define LCI_MATCHING_ENGINE_QUEUE_HPP

namespace lci
{
class matching_engine_queue_t : public matching_engine_impl_t
{
 public:
  matching_engine_queue_t(attr_t attr) : matching_engine_impl_t(attr) {}
  ~matching_engine_queue_t() = default;
  val_t insert(key_t key, val_t value, insert_type_t type) override
  {
    val_t ret = nullptr;
    entry_t entry = {key, value};
    lock.lock();
    if (type == insert_type_t::send) {
      // Search the posted recv queue for a matching entry
      ret = search(recv_queue, key);
      if (ret == nullptr) {
        // Did not find a matching posted recv
        send_queue.push_back(entry);
      }
    } else {
      // Search the unexpected send queue for a matching entry
      ret = search(send_queue, key);
      if (ret == nullptr) {
        // Did not find a matching incoming send
        recv_queue.push_back(entry);
      }
    }
    LCI_DBG_Log(LOG_TRACE, "matching_engine",
                "insert: key=%lu, value=%p, type=%d, ret=%p\n", key, value,
                (int)type, ret);
    lock.unlock();
    return ret;
  }

 private:
  struct entry_t {
    key_t key;
    val_t value;
  };
  std::list<entry_t> send_queue;
  std::list<entry_t> recv_queue;
  LCIU_CACHE_PADDING(sizeof(std::list<entry_t>) * 2);  // padding for cache line
  spinlock_t lock;
  LCIU_CACHE_PADDING(sizeof(spinlock_t));
  val_t search(std::list<entry_t>& queue, key_t key)
  {
    // the lock should be held by the caller
    for (auto it = queue.begin(); it != queue.end(); it++) {
      if (it->key == key) {
        // remove the entry
        queue.erase(it);
        return it->value;
      }
    }
    return nullptr;
  }
};

}  // namespace lci

#endif  // LCI_MATCHING_ENGINE_QUEUE_HPP