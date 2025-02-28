#ifndef LCI_RHANDLER_REGISTRY_H
#define LCI_RHANDLER_REGISTRY_H

namespace lci
{
class rhandler_registry_t
{
 public:
  enum class type_t {
    none,
    comp,
    matching_engine,
  };
  struct entry_t {
    type_t type;
    void* value;
  };

  rhandler_registry_t() = default;
  ~rhandler_registry_t() = default;

  // this is not thread-safe
  uint32_t register_rhandler(entry_t entry)
  {
    entries.push_back(entry);
    return entries.size() - 1;
  }

  // this is not thread-safe
  void deregister_rhandler(uint32_t idx)
  {
    LCI_Assert(idx < entries.size(), "idx (%u) >= entries.size() (%lu)\n", idx,
               entries.size());
    entries[idx] = {type_t::none, nullptr};
  }

  entry_t get(uint32_t idx)
  {
    LCI_Assert(idx < entries.size(), "idx (%u) >= entries.size() (%lu)\n", idx,
               entries.size());
    return entries[idx];
  }

 private:
  std::vector<entry_t> entries;
};

}  // namespace lci

#endif  // LCI_RHANDLER_REGISTRY_H