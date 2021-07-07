#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

int total = TOTAL;
int skip = SKIP;

void* buf;

static void* alloc(void* ctx, size_t size)
{
  return buf;
}

int main(int argc, char** args) {
  LCI_initialize(&argc, &args);
  LCI_endpoint_t ep;
  LCI_PL_t prop;
  LCI_MT_t mt;
  LCI_CQ_t cq;
  LCI_PL_create(&prop);
  LCI_MT_create(0, &mt);
  LCI_PL_set_comm_type(LCI_COMM_1SIDED, &prop);
  LCI_PL_set_allocator(&alloc, &prop);
  LCI_CQ_create(5, &cq);
  LCI_PL_set_completion(LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE, &prop);
  LCI_PL_set_cq(&cq, &prop);
  LCI_PL_set_mt(&mt, &prop);
  LCI_endpoint_create(0, prop, &ep);

  int rank = LCI_RANK;
  int tag = {99};

  LCI_request_t req;
  LCI_syncl_t sync;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* sbuf;
  posix_memalign(&buf, alignment, MAX_MSG);
  posix_memalign(&sbuf, alignment, MAX_MSG);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(sbuf, 'a', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        tag = i;
        LCI_one2one_set_empty(&sync);
        while(LCI_sendd(
          sbuf,
          size,
          1-rank,
          tag,
          ep,
          &sync
        ) != LCI_OK) {
          LCI_progress(0,1);
        }
        while(LCI_one2one_test_empty(&sync)) {
          LCI_progress(0,1);
        };

        LCI_request_t* req_ptr;
        while(LCI_CQ_dequeue(&cq, &req_ptr) != LCI_OK) {
          LCI_progress(0,1);
        }
        assert(req_ptr->tag == i);
        LCI_bbuffer_free(req_ptr->buffer.bbuffer,0);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(sbuf, 'a', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        LCI_request_t* req_ptr;
        while(LCI_CQ_dequeue(&cq, &req_ptr) != LCI_OK) {
          LCI_progress(0,1);
        }
        assert(req_ptr->tag == i);
        LCI_bbuffer_free(req_ptr->buffer.bbuffer,0);
        tag = i;
        LCI_one2one_set_empty(&sync);
        while(LCI_sendd(
          sbuf,
          size,
          1-rank,
          tag,
          ep,
          &sync
        ) != LCI_OK) {
          LCI_progress(0,1);
        }
        while(LCI_one2one_test_empty(&sync)) {
          LCI_progress(0,1);
        };
      }
    }
  }
  LCI_finalize();
}
