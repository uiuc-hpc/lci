/* -*- C -*-
 *
 * Copyright 2006 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
 * retains certain rights in this software.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <mpiv.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* constants */
const int magic_tag = 1;

/* configuration parameters - setable by command line arguments */
int npeers = 6;
int niters = 4096;
int nmsgs = 128;
int nbytes = 8;
int cache_size = (8 * 1024 * 1024 / sizeof(int));
int ppn = -1;
int machine_output = 0;
int threads = 0;
int rma_mode = 0;

/* globals */
int* send_peers;
int* recv_peers;
int* cache_buf;
char* send_buf;
char* recv_buf;
MPIV_Request* reqs;

int rank = -1;
int world_size = -1;

typedef struct {
  int thread_id;
  int iters;
  MPIV_Request* local_reqs;
  int rank;
  char* send_buf;
  //  MPI_Comm *comm;
} thread_input;

static void abort_app(const char* msg) {
  perror(msg);
  MPI_Abort(MPI_COMM_WORLD, 1);
}

static void cache_invalidate(void) {
  int i;

  cache_buf[0] = 1;
  for (i = 1; i < cache_size; ++i) {
    cache_buf[i] = cache_buf[i - 1];
  }
}

static inline double timer(void) { return MPI_Wtime(); }

void display_result(const char* test, const double result) {
  if (0 == rank) {
    if (machine_output) {
      printf("%.2f ", result);
    } else {
      printf("%10s: %.2f\n", test, result);
    }
  }
}

/*********************Start RMA FUNCS**************************/

void setupwindows() {}

/***********************End RMA FUNCS**************************/

void sendmsg(intptr_t input) {
  int nreqs = 0;
  int niters = 0;
  thread_input* t_info = (thread_input*)input;
  for (niters = 0; niters < t_info->iters; niters++) {
    MPIV_Isend(send_buf + (nbytes * niters), nbytes, MPI_CHAR,
               t_info->rank + (world_size / 2),
               t_info->thread_id * t_info->iters + niters, MPI_COMM_WORLD,
               &t_info->local_reqs[nreqs++]);
  }
  MPIV_Waitall(nreqs, t_info->local_reqs, MPI_STATUSES_IGNORE);
}

void recvmsg(intptr_t input) {
  int nreqs = 0;
  int niters = 0;
  thread_input* t_info = (thread_input*)input;
  for (niters = 0; niters < t_info->iters; niters++) {
    MPIV_Irecv(send_buf + (nbytes * niters), nbytes, MPI_CHAR,
               t_info->rank - (world_size / 2),
               t_info->thread_id * t_info->iters + niters, MPI_COMM_WORLD,
               &t_info->local_reqs[nreqs++]);
  }
  MPIV_Waitall(nreqs, t_info->local_reqs, MPI_STATUSES_IGNORE);
}

void test_one_way(void) {
  int i, k, nreqs;
  double tmp, total = 0;
  MPIV_Barrier(MPI_COMM_WORLD);
  thread_input* info;
  info = (thread_input*)malloc(threads * sizeof(thread_input) * 2);
  fult_t thread[threads];
  reqs = (MPIV_Request*)malloc(sizeof(MPIV_Request) * 2 * nmsgs * npeers *
                               threads);
  if (!(world_size % 2 == 1 && rank == (world_size - 1))) {
    if (rank < world_size / 2) {
      for (i = 0; i < niters; ++i) {
        cache_invalidate();
        MPI_Barrier(MPI_COMM_WORLD);
        nreqs = 0;
        int thread_count = 0;
        tmp = timer();
        for (thread_count = 0; thread_count < threads; thread_count++) {
          info[thread_count].thread_id = thread_count;
          info[thread_count].iters = nmsgs / threads;
          info[thread_count].local_reqs =
              &reqs[(nmsgs / threads) * thread_count];
          info[thread_count].rank = rank;
          // pthread_create(&pthreads[thread_count], NULL, sendmsg, (void
          // *)&info[thread_count]);
          thread[thread_count] =
              MPIV_spawn(thread_count, sendmsg, (intptr_t)&info[thread_count]);
        }
        for (thread_count = 0; thread_count < threads; thread_count++) {
          MPIV_join(thread[thread_count]);
        }
        total += (timer() - tmp);
        MPI_Barrier(MPI_COMM_WORLD);
      }
    } else {
      for (i = 0; i < niters; ++i) {
        cache_invalidate();
        MPI_Barrier(MPI_COMM_WORLD);
        nreqs = 0;
        int err;
        int thread_count = 0;
        tmp = timer();
        for (thread_count = 0; thread_count < threads; thread_count++) {
          info[thread_count].thread_id = thread_count;
          info[thread_count].iters = nmsgs / threads;
          info[thread_count].local_reqs =
              &reqs[(nmsgs / threads) * thread_count];
          info[thread_count].rank = rank;
          thread[thread_count] =
              MPIV_spawn(thread_count, recvmsg, (intptr_t)&info[thread_count]);
        }
        for (thread_count = 0; thread_count < threads; thread_count++) {
          MPIV_join(thread[thread_count]);
        }
        total += (timer() - tmp);
        MPI_Barrier(MPI_COMM_WORLD);
      }
    }
    MPI_Allreduce(&total, &tmp, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    display_result(
        "single direction ",
        (niters * nmsgs * ((world_size - 1) / 2)) / (tmp / world_size));
  }
  free(info);
  free(reqs);
  MPIV_Barrier(MPI_COMM_WORLD);
}

void same_dir_run(intptr_t input) {
  int nreqs = 0;
  int niters = 0;
  int j = 0;
  thread_input* t_info = (thread_input*)input;
  for (j = 0; j < npeers; ++j) {
    nreqs = 0;
    for (niters = 0; niters < t_info->iters; ++niters) {
      MPIV_Irecv(recv_buf + (nbytes * (niters + j * nmsgs)), nbytes, MPI_CHAR,
                 recv_peers[j], niters + t_info->thread_id * t_info->iters,
                 MPI_COMM_WORLD, &t_info->local_reqs[nreqs++]);
    }
    for (niters = 0; niters < t_info->iters; ++niters) {
      MPIV_Isend(send_buf + (nbytes * (niters + j * nmsgs)), nbytes, MPI_CHAR,
                 send_peers[npeers - j - 1],
                 niters + t_info->thread_id * t_info->iters, MPI_COMM_WORLD,
                 &t_info->local_reqs[nreqs++]);
    }
    MPIV_Waitall(nreqs, t_info->local_reqs, MPI_STATUSES_IGNORE);
  }
}

void test_same_dir(void) {
  int i, k, nreqs;
  double tmp, total = 0;
  MPI_Barrier(MPI_COMM_WORLD);

  thread_input* info;
  info = (thread_input*)malloc(threads * sizeof(thread_input) * 2);
  pthread_t* pthreads;
  fult_t* thread = new fult_t[threads];
  reqs = (MPIV_Request*)malloc(sizeof(MPIV_Request) * 2 * nmsgs * npeers *
                               threads);
  for (i = 0; i < niters; ++i) {
    cache_invalidate();
    MPI_Barrier(MPI_COMM_WORLD);
    nreqs = 0;
    int thread_count = 0;
    tmp = timer();
    for (thread_count = 0; thread_count < threads; thread_count++) {
      info[thread_count].thread_id = thread_count;
      info[thread_count].iters = nmsgs / threads;
      info[thread_count].local_reqs =
          &reqs[2 * (nmsgs / threads) * thread_count];
      info[thread_count].rank = rank;
      thread[thread_count] =
          MPIV_spawn(thread_count, same_dir_run, (intptr_t)&info[thread_count]);
    }
    for (thread_count = 0; thread_count < threads; thread_count++) {
      MPIV_join(thread[thread_count]);
    }
    total += (timer() - tmp);
  }
  MPI_Allreduce(&total, &tmp, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  display_result("pair-based       ",
                 (niters * npeers * nmsgs * 2) / (tmp / world_size));
  free(info);
  free(thread);
  free(reqs);
  MPI_Barrier(MPI_COMM_WORLD);
}

void test_same_direction(void) {
  int i, j, k, nreqs;
  double tmp, total = 0;

  MPIV_Barrier(MPI_COMM_WORLD);

  for (i = 0; i < niters; ++i) {
    cache_invalidate();

    MPIV_Barrier(MPI_COMM_WORLD);

    tmp = timer();
    for (j = 0; j < npeers; ++j) {
      nreqs = 0;
      for (k = 0; k < nmsgs; ++k) {
        MPIV_Irecv(recv_buf + (nbytes * (k + j * nmsgs)), nbytes, MPI_CHAR,
                   recv_peers[j], magic_tag, MPI_COMM_WORLD, &reqs[nreqs++]);
      }
      for (k = 0; k < nmsgs; ++k) {
        MPIV_Isend(send_buf + (nbytes * (k + j * nmsgs)), nbytes, MPI_CHAR,
                   send_peers[npeers - j - 1], magic_tag, MPI_COMM_WORLD,
                   &reqs[nreqs++]);
      }
      MPIV_Waitall(nreqs, reqs, MPI_STATUSES_IGNORE);
    }
    total += (timer() - tmp);
  }

  MPI_Allreduce(&total, &tmp, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  display_result("pair-based       ",
                 (niters * npeers * nmsgs * 2) / (tmp / world_size));
}

void* all_run(void* input) {
  int nreqs = 0;
  int niters = 0;
  int j = 0;
  thread_input* t_info = (thread_input*)input;
  for (j = 0; j < npeers; ++j) {
    for (niters = 0; niters < t_info->iters; ++niters) {
      MPIV_Irecv(recv_buf + (nbytes * (niters + j * nmsgs)), nbytes, MPI_CHAR,
                 recv_peers[j], magic_tag, MPI_COMM_WORLD,
                 &t_info->local_reqs[nreqs++]);
    }
    for (niters = 0; niters < t_info->iters; ++niters) {
      MPIV_Isend(send_buf + (nbytes * (niters + j * nmsgs)), nbytes, MPI_CHAR,
                 send_peers[npeers - j - 1], magic_tag, MPI_COMM_WORLD,
                 &t_info->local_reqs[nreqs++]);
    }
  }
  MPIV_Waitall(nreqs, t_info->local_reqs, MPI_STATUSES_IGNORE);
  return NULL;
}

void test_all(void) {
  int i, k, nreqs;
  double tmp, total = 0;
  MPIV_Barrier(MPI_COMM_WORLD);

  thread_input* info;
  info = (thread_input*)malloc(threads * sizeof(thread_input) * 2);
  pthread_t* pthreads;
  pthreads = (pthread_t*)malloc(threads * sizeof(pthreads) * 5);
  reqs = (MPIV_Request*)malloc(sizeof(MPIV_Request) * 2 * nmsgs * npeers *
                               threads);
  char* local_sendbuf = (char*)malloc(npeers * nmsgs * nbytes);
  for (i = 0; i < niters; ++i) {
    cache_invalidate();
    MPIV_Barrier(MPI_COMM_WORLD);
    nreqs = 0;
    int thread_count = 0;
    tmp = timer();
    for (thread_count = 0; thread_count < threads; thread_count++) {
      info[thread_count].thread_id = thread_count;
      info[thread_count].iters = nmsgs / threads;
      info[thread_count].local_reqs =
          &reqs[2 * (nmsgs / threads) * npeers * thread_count];
      info[thread_count].rank = rank;
      pthread_create(&pthreads[thread_count], NULL, all_run,
                     (void*)&info[thread_count]);
    }
    for (thread_count = 0; thread_count < threads; thread_count++) {
      pthread_join(pthreads[thread_count], NULL);
    }
    total += (timer() - tmp);
  }
  MPI_Allreduce(&total, &tmp, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  display_result("all-start        ",
                 (niters * npeers * nmsgs * 2) / (tmp / world_size));
  free(info);
  free(pthreads);
  free(reqs);
  MPIV_Barrier(MPI_COMM_WORLD);
}

void test_allstart(void) {
  int i, j, k, nreqs = 0;
  double tmp, total = 0;

  MPIV_Barrier(MPI_COMM_WORLD);

  for (i = 0; i < niters; ++i) {
    cache_invalidate();

    MPIV_Barrier(MPI_COMM_WORLD);

    tmp = timer();
    nreqs = 0;
    for (j = 0; j < npeers; ++j) {
      for (k = 0; k < nmsgs; ++k) {
        MPIV_Irecv(recv_buf + (nbytes * (k + j * nmsgs)), nbytes, MPI_CHAR,
                   recv_peers[j], magic_tag, MPI_COMM_WORLD, &reqs[nreqs++]);
      }
      for (k = 0; k < nmsgs; ++k) {
        MPIV_Isend(send_buf + (nbytes * (k + j * nmsgs)), nbytes, MPI_CHAR,
                   send_peers[npeers - j - 1], magic_tag, MPI_COMM_WORLD,
                   &reqs[nreqs++]);
      }
    }
    MPIV_Waitall(nreqs, reqs, MPI_STATUSES_IGNORE);
    total += (timer() - tmp);
  }

  MPI_Allreduce(&total, &tmp, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  display_result("all-start        ",
                 (niters * npeers * nmsgs * 2) / (tmp / world_size));
}

void usage(void) {
  fprintf(stderr, "Usage: msgrate -n <ppn> [OPTION]...\n\n");
  fprintf(stderr, "  -h           Display this help message and exit\n");
  fprintf(stderr, "  -p <num>     Number of peers used in communication\n");
  fprintf(stderr, "  -i <num>     Number of iterations per test\n");
  fprintf(stderr, "  -m <num>     Number of messages per peer per iteration\n");
  fprintf(stderr, "  -s <size>    Number of bytes per message\n");
  fprintf(stderr, "  -c <size>    Cache size in bytes\n");
  fprintf(stderr, "  -n <ppn>     Number of procs per node\n");
  fprintf(stderr, "  -o           Format output to be machine readable\n");
  fprintf(stderr, "\nReport bugs to <bwbarre@sandia.gov>\n");
}

int main(int argc, char* argv[]) {
  int start_err = 0;
  int i;
  int prov;

  MPIV_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  /* root handles arguments and bcasts answers */
  if (0 == rank) {
    int ch;
    while (start_err != 1 &&
           (ch = getopt(argc, argv, "p:i:m:s:c:n:o:t:r:u:h")) != -1) {
      switch (ch) {
        case 'p':
          npeers = atoi(optarg);
          break;
        case 'i':
          niters = atoi(optarg);
          break;
        case 'm':
          nmsgs = atoi(optarg);
          break;
        case 's':
          nbytes = atoi(optarg);
          break;
        case 'c':
          cache_size = atoi(optarg) / sizeof(int);
          break;
        case 'n':
          ppn = atoi(optarg);
          break;
        case 'o':
          machine_output = 1;
          break;
        case 't':
          threads = atoi(optarg);
          break;
        case 'r':
          rma_mode = atoi(optarg);
          break;
        case 'h':
        case '?':
        default:
          start_err = 1;
          usage();
      }
    }

    /* sanity check */
    if (start_err != 1) {
      if (world_size < 3) {
        fprintf(stderr, "Error: At least three processes are required\n");
        start_err = 1;
      } else if (world_size <= npeers) {
        fprintf(stderr, "Error: job size (%d) <= number of peers (%d)\n",
                world_size, npeers);
        start_err = 1;
      } else if (ppn < 1) {
        fprintf(stderr, "Error: must specify process per node (-n #)\n");
        start_err = 1;
      } else if ((double)world_size / (double)ppn <= (double)npeers) {
        fprintf(stderr, "Error: node count <= number of peers %i / %i <= %i\n",
                world_size, ppn, npeers);
        start_err = 1;
      }
    }
  }

  /* broadcast results */
  MPI_Bcast(&start_err, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (0 != start_err) {
    MPI_Finalize();
    exit(1);
  }
  MPI_Bcast(&npeers, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&niters, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&nmsgs, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&nbytes, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&cache_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&ppn, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&threads, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (0 == rank) {
    if (!machine_output) {
      printf("job size         : %d\n", world_size);
      printf("npeers           : %d\n", npeers);
      printf("niters           : %d\n", niters);
      printf("nmsgs            : %d\n", nmsgs);
      printf("nbytes           : %d\n", nbytes);
      printf("cache size       : %d\n", cache_size * (int)sizeof(int));
      printf("ppn              : %d\n", ppn);
      printf("threads          : %d\n", threads);
      printf("RMA Mode         : ");
      switch (rma_mode) {
        case 0:
          printf("no rma\n");
          break;
        case 1:
          printf("lock/unlock\n");
          break;
        case 2:
          printf("fence\n");
          break;
        case 3:
          printf("pswc\n");
          break;
      }
    } else {
      printf("%d %d %d %d %d %d %d ", world_size, npeers, niters, nmsgs, nbytes,
             cache_size * (int)sizeof(int), ppn);
    }
  }

  /* allocate buffers */
  send_peers = (int*)mpiv_malloc(sizeof(int) * npeers);
  if (NULL == send_peers) abort_app("malloc");
  recv_peers = (int*)mpiv_malloc(sizeof(int) * npeers);
  if (NULL == recv_peers) abort_app("malloc");
  cache_buf = (int*)mpiv_malloc(sizeof(int) * cache_size);
  if (NULL == cache_buf) abort_app("malloc");
  send_buf = (char*)mpiv_malloc(npeers * nmsgs * nbytes);
  if (NULL == send_buf) abort_app("malloc");

  // bad_buf = malloc(npeers * nmsgs * nbytes);
  // if (NULL == send_buf) abort_app("malloc");

  recv_buf = (char*)mpiv_malloc(npeers * nmsgs * nbytes);
  if (NULL == recv_buf) abort_app("malloc");
  reqs = (MPIV_Request*)malloc(sizeof(MPIV_Request) * 2 * nmsgs * npeers);
  if (NULL == reqs) abort_app("malloc");

  /* calculate peers */
  for (i = 0; i < npeers; ++i) {
    if (i < npeers / 2) {
      send_peers[i] =
          (rank + world_size + ((i - npeers / 2) * ppn)) % world_size;
    } else {
      send_peers[i] =
          (rank + world_size + ((i - npeers / 2 + 1) * ppn)) % world_size;
    }
  }
  if (npeers % 2 == 0) {
    /* even */
    for (i = 0; i < npeers; ++i) {
      if (i < npeers / 2) {
        recv_peers[i] =
            (rank + world_size + ((i - npeers / 2) * ppn)) % world_size;
      } else {
        recv_peers[i] =
            (rank + world_size + ((i - npeers / 2 + 1) * ppn)) % world_size;
      }
    }
  } else {
    /* odd */
    for (i = 0; i < npeers; ++i) {
      if (i < npeers / 2 + 1) {
        recv_peers[i] =
            (rank + world_size + ((i - npeers / 2 - 1) * ppn)) % world_size;
      } else {
        recv_peers[i] =
            (rank + world_size + ((i - npeers / 2) * ppn)) % world_size;
      }
    }
  }

  /* BWB: FIX ME: trash the free lists / malloc here */

  /* sync, although tests will do this on their own (in theory) */
  MPI_Barrier(MPI_COMM_WORLD);

  MPIV_Init_worker(threads);
  MPIV_Finalize();
  if (rank == 0 && machine_output) printf("\n");
  return 0;
}

void main_task(intptr_t) {
  /* run tests */
  test_one_way();
  test_same_dir();
  // test_same_direction();
  // test_all();
  // test_allstart();
}
