#ifndef LCI_RCOMP_REGISTRY_HPP
#define LCI_RCOMP_REGISTRY_HPP

namespace lci
{
class rcomp_registry_t
{
 public:
  rcomp_registry_t() = default;
  ~rcomp_registry_t() = default;

  // this is not thread-safe
  rcomp_t register_rcomp(comp_t comp)
  {
    comps.push_back(comp);
    return comps.size() - 1;
  }

  // this is not thread-safe
  void deregister_rcomp(rcomp_t rcomp)
  {
    LCI_Assert(rcomp < comps.size(), "rcomp (%u) >= rcomps.size() (%lu)\n",
               rcomp, comps.size());
    comps[rcomp].p_impl = nullptr;
  }

  comp_t get(rcomp_t rcomp)
  {
    LCI_Assert(rcomp < comps.size(), "rcomp (%u) >= rcomps.size() (%lu)\n",
               rcomp, comps.size());
    return comps[rcomp];
  }

 private:
  std::vector<comp_t> comps;
};

}  // namespace lci

#endif  // LCI_RCOMP_REGISTRY_HPP