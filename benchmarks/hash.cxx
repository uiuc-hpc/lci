#define CONFIG_TABLE_SIZE 16

#include "hashtable/hashtbl.h"
#include "hashtable/hashtbl_arr.h"
#include "hashtable/hashtbl_tbb.h"
#include "hashtable/hashtbl_cock.h"

#include "comm_exp.h"
#include "affinity.h"

#include <thread>
#include <atomic>
#include <iostream>

// We are only interested in thread with 2 worker.
#define BARRIER(f, n) { f++; while (f < n) { } }

int NUM_INSERTED;
int NUM_INSERTED_PER_THREAD = 32;
int NTHREADS = 1;

enum type_t {
  NONE, 
  SERVER,
  THREADS,
};

int *cache_buf;
int cache_size;

static void
cache_invalidate(void)
{
  int i;
  cache_buf[0] = 1;
  for (i = 1 ; i < cache_size ; ++i) {
    cache_buf[i] = cache_buf[i - 1];
  }
}

typedef std::function<void(mv_hash**)> hash_init_f ;
typedef std::function<bool(mv_hash*, const mv_key&, mv_value&)> hash_insert_f;

template<typename HASH_T, type_t whofirst>
void benchmark_insert_with_delete(hash_init_f init, hash_insert_f insert) {
  std::cout << typeid(HASH_T).name() << " " << whofirst << std::endl;
  mv_hash* my_table;
  init(&my_table);

  double ti1, ti2;
  ti1 = ti2 = 0;
  std::atomic<int> f1, f2;

  std::vector<double> times(TOTAL_LARGE - SKIP_LARGE, 0.0);

  affinity::set_me_to_(0);
  for (int j=0; j<TOTAL_LARGE; j++)  {
    cache_invalidate();
    f1 = f2 = 0;
    auto t1 = std::thread([&]{
        affinity::set_me_to_(0);
        mv_value v = 1;
        BARRIER(f1, NTHREADS + 1)
        if (whofirst == THREADS) BARRIER(f2, NTHREADS + 1)
        if (j >= SKIP_LARGE) ti1 -= wtime();
        for (int i = 0; i < NUM_INSERTED; i++) {
          bool ret = insert(my_table, (mv_key) i, v);
          if (whofirst == SERVER) assert(ret); else assert(!ret);
        }
        if (j >= SKIP_LARGE) ti1 += wtime();
        if (whofirst == SERVER) BARRIER(f2, NTHREADS + 1);
    });

    std::vector<std::thread> th(NTHREADS);
 
    for (int tt = 0; tt < NTHREADS; tt++) {
    th[tt] = std::move(std::thread([tt, j, &insert, &times, &my_table, &ti2, &f1, &f2] {
        affinity::set_me_to_(tt + 1);
        mv_value v = 2;
        BARRIER(f1, NTHREADS + 1)
        if (whofirst == SERVER) BARRIER(f2, NTHREADS + 1);
        if (j >= SKIP_LARGE && tt == 0) times[j - SKIP_LARGE] -= wtime();
        for (int i = tt; i < NUM_INSERTED; i+=NTHREADS) {
          bool ret = insert(my_table, (mv_key) i, v);
          if (whofirst == SERVER) assert(!ret); else assert(ret);
        }
        if (j >= SKIP_LARGE && tt == 0) times[j - SKIP_LARGE] += wtime();
        if (whofirst == THREADS) BARRIER(f2, NTHREADS + 1);
    }));
    }

    for (auto& t : th) t.join();
    t1.join();

  }
  
  // double min = 1e6*(*(std::min_element(times.begin(), times.end()))) / (NUM_INSERTED_PER_THREAD)/(TOTAL_LARGE - SKIP_LARGE);
  // double max = 1e6*(*(std::max_element(times.begin(), times.end()))) / (NUM_INSERTED_PER_THREAD)/(TOTAL_LARGE - SKIP_LARGE);
  // compute - 5 quantile:
  int size = times.size();
  std::sort(times.begin(), times.end());
  std::vector<double> qu(5, 0.0);
  qu[0] = (size % 2==1)?(times[size/2]):((times[size/2-1] + times[size/2])/2); // median
  qu[1] = times[size*3/4]; //u q
  qu[2] = times[size/4]; //d q
  double iq = qu[1] - qu[2];
  qu[3] = std::min(qu[1] + 1.5*iq, times[size - 1]); // max
  qu[4] = std::max(qu[2] - 1.5*iq, times[0]); // min

  for (auto &q : qu) q = q * 1e6 / (NUM_INSERTED_PER_THREAD);// / (TOTAL_LARGE - SKIP_LARGE);

  if (whofirst == SERVER) {
    printf("Time insert (server): %.3f\n", 1e6 * ti1/(NUM_INSERTED)/(TOTAL_LARGE - SKIP_LARGE));
    printf("Time find+erase (thread): %d %.3f %.3f %.3f %.3f %.3f\n", NTHREADS, qu[0], qu[1], qu[2], qu[3], qu[4]);
  } else if (whofirst == THREADS) {
    printf("Time find+erase (server): %.3f\n", 1e6 * ti1/(NUM_INSERTED)/(TOTAL_LARGE - SKIP_LARGE));
    printf("Time insert (thread): %d %.3f %.3f %.3f %.3f %.3f\n", NTHREADS, qu[0], qu[1], qu[2], qu[3], qu[4]);
  } else {
    printf("Time ops (server): %.3f\n", 1e6 * ti1/(NUM_INSERTED)/(TOTAL_LARGE - SKIP_LARGE));
    printf("Time ops (thread): %d %.3f %.3f %.3f %.3f %.3f\n", NTHREADS, qu[0], qu[1], qu[2], qu[3], qu[4]);
  }
}

int main(int argc, char** args) {
  if (argc > 1)
    NUM_INSERTED_PER_THREAD = atoi(args[1]);
  if (argc > 2)
    NTHREADS = atoi(args[2]);

  NUM_INSERTED = NUM_INSERTED_PER_THREAD * NTHREADS;

  printf("Nthreads: %d, Server: %d, per-thread: %d\n", NTHREADS, NUM_INSERTED, NUM_INSERTED_PER_THREAD);

#if 0
  benchmark_insert_with_delete<arr_hashtbl, NONE>();
  benchmark_insert_with_delete<cock_hashtbl, NONE>();
  // benchmark_insert_with_delete<tbb_hashtbl, NONE>();
#endif
  cache_size = (8 * 1024 * 1024 / sizeof(int));
  cache_buf = (int*) malloc(sizeof(int) * cache_size);

  benchmark_insert_with_delete<arr_hash_val, SERVER>(arr_hash_init, arr_hash_insert);
  benchmark_insert_with_delete<ck_hash_val, SERVER>(ck_hash_init, ck_hash_insert);
  benchmark_insert_with_delete<tbb_hash_val, SERVER>(tbb_hash_init, tbb_hash_insert);

  benchmark_insert_with_delete<arr_hash_val, THREADS>(arr_hash_init, arr_hash_insert);
  benchmark_insert_with_delete<ck_hash_val, THREADS>(ck_hash_init, ck_hash_insert);
  benchmark_insert_with_delete<tbb_hash_val, THREADS>(tbb_hash_init, tbb_hash_insert);
  return 0;
}
