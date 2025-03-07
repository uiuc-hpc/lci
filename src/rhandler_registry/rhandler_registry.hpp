#ifndef LCI_RHANDLER_REGISTRY_H
#define LCI_RHANDLER_REGISTRY_H

namespace lci
{
// minimum rhandler is 1
class rhandler_registry_t
{
 public:
  enum class type_t {
    none,
    comp,
    matching_engine,
  };
  struct entry_t {
    type_t type = type_t::none;
    void* value = nullptr;
    uint64_t metadata = 0;
    entry_t() = default;
    entry_t(type_t type, void* value, uint64_t metadata = 0)
        : type(type), value(value), metadata(metadata)
    {
    }
  };

  rhandler_registry_t() = default;
  ~rhandler_registry_t() = default;

  uint32_t reserve(uint32_t n)
  {
    int idx = entries.size();
    entries.insert(entries.end(), n, entry_t());
    return encode_idx(idx);
  }

  void register_rhandler(uint32_t idx, entry_t entry)
  {
    idx = decode_idx(idx);
    LCI_Assert(idx < entries.size(), "idx (%u) >= entries.size() (%lu)\n", idx,
               entries.size());
    entries[idx] = entry;
  }

  // this is not thread-safe
  uint32_t register_rhandler(entry_t entry)
  {
    uint32_t idx = entries.size();
    entries.push_back(entry);
    return encode_idx(idx);
  }

  // this is not thread-safe
  void deregister_rhandler(uint32_t idx)
  {
    idx = decode_idx(idx);
    LCI_Assert(idx < entries.size(), "idx (%u) >= entries.size() (%lu)\n", idx,
               entries.size());
    entries[idx] = entry_t();
  }

  entry_t get(uint32_t idx)
  {
    idx = decode_idx(idx);
    LCI_Assert(idx < entries.size(), "idx (%u) >= entries.size() (%lu)\n", idx,
               entries.size());
    return entries[idx];
  }

 private:
  inline static uint32_t encode_idx(uint32_t idx) { return idx + 1; }
  inline static uint32_t decode_idx(uint32_t idx) { return idx - 1; }

  std::vector<entry_t> entries;
};

}  // namespace lci

#endif  // LCI_RHANDLER_REGISTRY_H