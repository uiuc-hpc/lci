#include "lci.h"
#include <cuda.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG (8 * 1024)

int total = TOTAL;
int skip = SKIP;

__global__ void kernel(void* dst, void* src, unsigned int count)
{
  unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
  unsigned char* d = (unsigned char*)dst;
  unsigned char* s = (unsigned char*)src;
  if (i < count)
      d[i] = s[i];
}

int main(int argc, char** args) {
  LCI_initialize(&argc, &args);
  LCI_endpoint_t ep;
  LCI_PL_t prop;
  LCI_PL_create(&prop);
  LCI_MT_t mt;
  LCI_MT_create(0, &mt);
  LCI_PL_set_mt(&mt, &prop);
  LCI_endpoint_create(0, prop, &ep);

  int rank = LCI_RANK;
  int tag = 99;

  LCI_syncl_t sync;

  double t1 = 0;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* src_buf = 0;
  void* dst_buf = 0;
  void* dst_gpu_buf = 0;
  cudaMalloc(&src_buf, MAX_MSG);
  posix_memalign(&dst_buf, alignment, MAX_MSG);
  cudaMalloc(&dst_gpu_buf, MAX_MSG);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      cudaMemset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        assert(cudaGetLastError() == cudaSuccess);
        if (i == skip) t1 = wtime();
        while (LCI_sendbc(src_buf, size, 1-rank, tag, ep) != LCI_OK)
          LCI_progress(0, 1);

        LCI_one2one_set_empty(&sync);
        LCI_recvbc(dst_buf, size, 1-rank, tag, ep, &sync);
        while (LCI_one2one_test_empty(&sync))
          LCI_progress(0, 1);

        cudaMemcpy(dst_gpu_buf, dst_buf, size, cudaMemcpyHostToDevice);
        kernel<<<max(size/64, 1), 64>>>(src_buf, dst_gpu_buf, size);
        cudaDeviceSynchronize();
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      cudaMemset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        assert(cudaGetLastError() == cudaSuccess);
        LCI_one2one_set_empty(&sync);
        LCI_recvbc(dst_buf, size, 1-rank, tag, ep, &sync);
        while (LCI_one2one_test_empty(&sync))
          LCI_progress(0, 1);

        cudaMemcpy(dst_gpu_buf, dst_buf, size, cudaMemcpyHostToDevice);
        kernel<<<max(size/64, 1), 64>>>(src_buf, dst_gpu_buf, size);
        cudaDeviceSynchronize();

        while (LCI_sendbc(src_buf, size, 1-rank, tag, ep) != LCI_OK)
          LCI_progress(0, 1);
      }
    }
  }
  cudaFree(src_buf);
  free(dst_buf);
  cudaFree(dst_gpu_buf);
  LCI_finalize();
}
