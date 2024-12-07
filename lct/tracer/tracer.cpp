#include <vector>
#include <fstream>
#include "lcti.hpp"

namespace lct::tracer
{
template <typename T>
struct is_bitwise_serializable : std::is_arithmetic<T> {
};

template <typename T>
inline constexpr bool is_bitwise_serializable_v =
    is_bitwise_serializable<T>::value;

struct basic_storage_t {
  char* ptr;
  size_t size;
  explicit basic_storage_t(size_t size_) : size(size_) { ptr = new char[size]; }
  basic_storage_t() { delete ptr; }

  template <typename T>
  bool save(size_t idx, const T& val)
  {
    if (idx + sizeof(val) > size) return false;
    memcpy(ptr + idx, &val, sizeof(val));
    return true;
  }

  template <typename T>
  bool load(size_t idx, T& val)
  {
    if (idx + sizeof(val) > size) return false;
    memcpy(&val, ptr + idx, sizeof(val));
    return true;
  }
};

struct storage_t {
  const size_t default_storage_size = 1 << 20;  // 1MB
  using bin_t = std::pair<size_t, basic_storage_t>;
  std::vector<bin_t> storages;

  storage_t()
  {
    storages.push_back({0, basic_storage_t(default_storage_size)});
  }

  size_t search(size_t idx)
  {
    // fast path
    if (idx >= storages.back().first) return storages.size() - 1;
    // binary search
    // Target are in [l, h]
    size_t l = 0, h = storages.size() - 2;
    while (l < h) {
      size_t m = (h + l) / 2;
      if (idx < storages[m].first) {
        h = m - 1;
      } else if (idx < storages[m + 1].first) {
        h = m;
        l = m;
      } else {
        l = m + 1;
      }
    }
    LCT_Assert(LCT_log_ctx_default,
               l >= 0 && l < storages.size() && storages[l].first <= idx &&
                   (l == storages.size() - 1 || idx < storages[l + 1].first),
               "Unexpected search result!\n");
    return l;
  }

  template <typename T>
  void save(size_t idx, const T& val)
  {
    size_t bin_idx = search(idx);
    bool succeed =
        storages[bin_idx].second.save(idx - storages[bin_idx].first, val);
    if (succeed) return;
    // Failed, need to move to the next bin
    ++bin_idx;
    if (bin_idx >= storages.size())
      storages.push_back(
          {idx, basic_storage_t(default_storage_size * (1UL << bin_idx))});
    bool ret =
        storages[bin_idx].second.save(idx - storages[bin_idx].first, val);
    LCT_Assert(LCT_log_ctx_default, ret,
               "The new storage cannot hold a single variable of size %lu\n",
               sizeof(val));
  }

  template <typename T>
  bool load(size_t& idx, T& val)
  {
    size_t bin_idx = search(idx);
    bool succeed =
        storages[bin_idx].second.load(idx - storages[bin_idx].first, val);
    if (succeed) return true;
    // Failed, need to move to the next bin
    ++bin_idx;
    if (bin_idx >= storages.size()) return false;
    bool ret =
        storages[bin_idx].second.load(idx - storages[bin_idx].first, val);
    LCT_Assert(LCT_log_ctx_default, ret,
               "The new storage cannot hold a single variable of size %lu\n",
               sizeof(val));
    return ret;
  }
};

struct output_archive_t {
  size_t idx;
  storage_t* storage;
  output_archive_t() = default;
  output_archive_t(storage_t* storage_) : idx(0), storage(storage_){};

  template <typename T>
  void operator&(const T& val)
  {
    if constexpr (is_bitwise_serializable_v<T>) {
      storage->save(idx, val);
      idx += sizeof(val);
    } else {
      val.serialize(*this, 0);
    }
  }

  size_t nbytes() { return idx; }
};

struct input_archive_t {
  size_t idx;
  storage_t* storage;

  input_archive_t(storage_t* storage_) : idx(0), storage(storage_){};

  template <typename T>
  void operator&(T& val)
  {
    if constexpr (is_bitwise_serializable_v<T>) {
      storage->load(idx, val);
      idx += sizeof(val);
    } else {
      val.serialize(*this, 0);
    }
  }

  size_t nbytes() { return idx; }
};

struct event_t {
  LCT_time_t time;
  enum class event_type_t {
    SEND,
    RECV,
  } event_type : 1;
  int32_t user_type : 31;
  int32_t rank;
  uint64_t size;
};
template <>
struct is_bitwise_serializable<event_t> : std::true_type {
};

struct tls_tracer_t {
  tls_tracer_t() { output_archive = output_archive_t(&storage); };
  storage_t storage;
  output_archive_t output_archive;
};

struct tracer_t {
  tracer_t(std::string name_, std::vector<std::string> typenames_, const char* filename_, bool write_binary_)
      : name(name_), typenames(typenames_), filename(filename_), write_binary(write_binary_), time_init(LCT_now()){};
  ~tracer_t(){};
  void record_send(int32_t user_type, int32_t rank, uint64_t size)
  {
    auto tracer_p = tls_tracer.get_or_allocate<tls_tracer_t>();
    event_t event = {.time = LCT_now() - time_init,
                     .event_type = event_t::event_type_t::SEND,
                     .user_type = user_type,
                     .rank = rank,
                     .size = size};
    tracer_p->output_archive& event;
  }
  void record_recv(int32_t user_type, int32_t rank, uint64_t size)
  {
    auto tracer_p = tls_tracer.get_or_allocate<tls_tracer_t>();
    event_t event = {.time = LCT_now() - time_init,
                     .event_type = event_t::event_type_t::RECV,
                     .user_type = user_type,
                     .rank = rank,
                     .size = size};
    tracer_p->output_archive& event;
  }
  
  void dump_binary(std::string ofilename) {
    std::ofstream outfile(ofilename, std::ofstream::binary);
    if (!outfile.is_open()) {
        fprintf(stderr, "Cannot open the logfile %s!\n", ofilename.c_str());
    }
    outfile << LCT_get_rank() << LCT_get_nranks() << LCT_time_to_s(LCT_now() - time_init);
    auto all_ptrs = tls_tracer.get_all();
    for (int i = 0; i < all_ptrs.size(); ++i) {
      auto tracer = reinterpret_cast<tls_tracer_t*>(all_ptrs[i]);
      if (tracer == nullptr) continue;
      outfile << i << tracer->output_archive.nbytes();
      input_archive_t ar(&tracer->storage);
      event_t event;
      while (ar.nbytes() < tracer->output_archive.nbytes()) {
        ar& event;
        outfile.write(reinterpret_cast<const char*>(&event), sizeof(event));
      }
    }
    outfile.close();
  }

  void dump_string(std::string ofilename) {
    FILE *outfile;
    if (ofilename == "stderr")
      outfile = stderr;
    else if (ofilename == "stdout")
      outfile = stdout;
    else {
      outfile = fopen(ofilename.c_str(), "a");
      if (outfile == nullptr) {
        fprintf(stderr, "Cannot open the logfile %s!\n", ofilename.c_str());
      }
    }

    fprintf(outfile, "lct::tracer::dump: rank %d/%d time %.9lf\n", LCT_get_rank(), LCT_get_nranks(), LCT_time_to_s(LCT_now() - time_init));
    auto all_ptrs = tls_tracer.get_all();
    for (int i = 0; i < all_ptrs.size(); ++i) {
      auto tracer = reinterpret_cast<tls_tracer_t*>(all_ptrs[i]);
      if (tracer == nullptr) continue;
      input_archive_t ar(&tracer->storage);
      event_t event;
      while (ar.nbytes() < tracer->output_archive.nbytes()) {
        ar& event;
        // print
        std::string event_type_str;
        switch (event.event_type) {
          case event_t::event_type_t::SEND:
            event_type_str = "send";
            break;
          case event_t::event_type_t::RECV:
            event_type_str = "recv";
            break;
        }
        fprintf(outfile, "%.9lf %d/%d: %s %d %d %lu\n",
                LCT_time_to_s(event.time), LCT_get_rank(), i,
                event_type_str.c_str(), event.user_type, event.rank,
                event.size);
      }
    }
  }

  void dump()
  {
    std::string ofilename =
            replaceOne(filename, "%", std::to_string(LCT_get_rank()));
    if (write_binary)
      dump_binary(ofilename);
    else
      dump_string(ofilename);
  }

 private:
  tlptr::tlptr_t tls_tracer;
  std::string name;
  std::vector<std::string> typenames;
  LCT_time_t time_init;
  const char* filename;
  bool write_binary;
};
}  // namespace lct::tracer

LCT_tracer_t LCT_tracer_init(char* name, const char* typenames[], int ntypes, const char* filename, bool write_binary)
{
  std::vector<std::string> typenames_;
  for (int i = 0; i < ntypes; ++i) {
    typenames_.emplace_back(typenames[i]);
  }
  auto* tracer = new lct::tracer::tracer_t(name, typenames_, filename, write_binary);
  return tracer;
}
void LCT_tracer_fina(LCT_tracer_t tracer)
{
  auto tracer_p = reinterpret_cast<lct::tracer::tracer_t*>(tracer);
  tracer_p->dump();
  delete tracer_p;
}
void LCT_tracer_send(LCT_tracer_t tracer, int32_t type, int32_t rank,
                     uint64_t size)
{
  auto tracer_p = reinterpret_cast<lct::tracer::tracer_t*>(tracer);
  tracer_p->record_send(type, rank, size);
}
void LCT_tracer_recv(LCT_tracer_t tracer, int32_t type, int32_t rank,
                     uint64_t size)
{
  auto tracer_p = reinterpret_cast<lct::tracer::tracer_t*>(tracer);
  tracer_p->record_recv(type, rank, size);
}