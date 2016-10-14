/** \file jacobi3d.C
 *  Author: Abhinav S Bhatele
 *  Date Created: December 19th, 2010
 *
 *        ***********  ^
 *      *         * *  |
 *    ***********   *  |
 *    *		*   *  Y
 *    *		*   *  |
 *    *		*   *  |
 *    *		*   *  ~
 *    *		* *
 *    ***********   Z
 *    <--- X --->
 *
 *    X: left, right --> wrap_x
 *    Y: top, bottom --> wrap_y
 *    Z: front, back --> wrap_z
 *
 *  Three dimensional decomposition of a 3D stencil
 */

#undef USE_AFFI
#define DISABLE_COMM
#define NWORKER 15
#if ((4096 / NWORKER) > (8*64))
#define USE_L1_MASK
#endif

#include "mpiv.h"
#include <utility>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>

/* We want to wrap entries around, and because mod operator % sometimes
 * misbehaves on negative values. -1 maps to the highest value.
 */
#define wrap_x(a) (((a) + num_blocks_x) % num_blocks_x)
#define wrap_y(a) (((a) + num_blocks_y) % num_blocks_y)
#define wrap_z(a) (((a) + num_blocks_z) % num_blocks_z)

#define index(a, b, c) \
  ((a) + (b) * (blockDimX + 2) + (c) * (blockDimX + 2) * (blockDimY + 2))
#define calc_pe(a, b, c) \
  ((a) + (b)*num_blocks_x + (c)*num_blocks_x * num_blocks_y)
 
#define get_i(X) ((X % ((blockDimX + 2) * (blockDimY + 2))) % (blockDimX + 2))
#define get_j(X) ((X % ((blockDimX + 2) * (blockDimY + 2))) / (blockDimX + 2))
#define get_k(X) ((X / ((blockDimX + 2) * (blockDimY + 2))))

#define MAX_ITER 200
#define SKIP_ITER 100
#define LEFT 1
#define RIGHT 2
#define TOP 3
#define BOTTOM 4
#define FRONT 5
#define BACK 6
#define DIVIDEBY7 0.14285714285714285714

double startTime;
double endTime;
int blockDimX, blockDimY, blockDimZ;
int arrayDimX, arrayDimY, arrayDimZ;
int noBarrier = 0;
int myRank, numPes;

int main(int argc, char** argv) {
  MPIV_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &numPes);
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

  if (argc != 4 && argc != 8) {
    printf("%s [array_size] [block_size] +[no]barrier\n", argv[0]);
    printf(
        "%s [array_size_X] [array_size_Y] [array_size_Z] [block_size_X] "
        "[block_size_Y] [block_size_Z] +[no]barrier\n",
        argv[0]);
    MPI_Abort(MPI_COMM_WORLD, -1);
  }

  if (argc == 4) {
    arrayDimZ = arrayDimY = arrayDimX = atoi(argv[1]);
    blockDimZ = blockDimY = blockDimX = atoi(argv[2]);
    if (strcasecmp(argv[3], "+nobarrier") == 0)
      noBarrier = 1;
    else
      noBarrier = 0;
    if (noBarrier && myRank == 0)
      printf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
  } else {
    arrayDimX = atoi(argv[1]);
    arrayDimY = atoi(argv[2]);
    arrayDimZ = atoi(argv[3]);
    blockDimX = atoi(argv[4]);
    blockDimY = atoi(argv[5]);
    blockDimZ = atoi(argv[6]);
    if (strcasecmp(argv[7], "+nobarrier") == 0)
      noBarrier = 1;
    else
      noBarrier = 0;
    if (noBarrier && myRank == 0)
      printf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
  }

  if (arrayDimX < blockDimX || arrayDimX % blockDimX != 0) {
    printf("array_size_X % block_size_X != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }
  if (arrayDimY < blockDimY || arrayDimY % blockDimY != 0) {
    printf("array_size_Y % block_size_Y != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }
  if (arrayDimZ < blockDimZ || arrayDimZ % blockDimZ != 0) {
    printf("array_size_Z % block_size_Z != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }

  MPIV_Init_worker(NWORKER);
  MPIV_Finalize();
}

double* left_plane_out;
double* right_plane_out;
double* left_plane_in;
double* right_plane_in;
double* bottom_plane_out;
double* top_plane_out;
double* bottom_plane_in;
;
double* top_plane_in;
double* back_plane_out;
double* front_plane_out;
double* back_plane_in;
double* front_plane_in;

double* temperature;
double* new_temperature;

int num_blocks_x;
int num_blocks_y;
int num_blocks_z;
int myXcoord;
int myYcoord;
int myZcoord;

void right(intptr_t) {
  MPI_Recv(right_plane_in, blockDimY * blockDimZ, MPI_DOUBLE,
            calc_pe(wrap_x(myXcoord + 1), myYcoord, myZcoord), RIGHT,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#if 0
  for(int k=0; k<blockDimZ; ++k)
    for(int j=0; j<blockDimY; ++j) {
      temperature[index(blockDimX+1, j+1, k+1)] = right_plane_in[k*blockDimY+j];
    }
#endif
  int k, j, i;
  i = blockDimX;
  for (k = 1; k < blockDimZ + 1; k++)
    for (j = 1; j < blockDimY + 1; j++) {
      new_temperature[index(i, j, k)] =
          (temperature[index(i - 1, j, k)] +
           right_plane_in[(k - 1) * blockDimY + (j - 1)] +
           temperature[index(i, j - 1, k)] + temperature[index(i, j + 1, k)] +
           temperature[index(i, j, k - 1)] + temperature[index(i, j, k + 1)] +
           temperature[index(i, j, k)]) *
          DIVIDEBY7;
    }
}

void left(intptr_t) {
  MPI_Recv(left_plane_in, blockDimY * blockDimZ, MPI_DOUBLE,
            calc_pe(wrap_x(myXcoord - 1), myYcoord, myZcoord), LEFT,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
/* Copy buffers into ghost layers */

#if 0
  for(int k=0; k<blockDimZ; ++k)
    for(int j=0; j<blockDimY; ++j) {
      temperature[index(0, j+1, k+1)] = left_plane_in[k*blockDimY+j];
    }
#endif
  int k, j, i;
  i = 1;
  for (k = 1; k < blockDimZ + 1; k++)
    for (j = 1; j < blockDimY + 1; j++) {
      new_temperature[index(i, j, k)] =
          (left_plane_in[(k - 1) * blockDimY + (j - 1)] +
           temperature[index(i + 1, j, k)] + temperature[index(i, j - 1, k)] +
           temperature[index(i, j + 1, k)] + temperature[index(i, j, k - 1)] +
           temperature[index(i, j, k + 1)] + temperature[index(i, j, k)]) *
          DIVIDEBY7;
    }
}

void up(intptr_t) {
  MPI_Recv(top_plane_in, blockDimX * blockDimZ, MPI_DOUBLE,
            calc_pe(myXcoord, wrap_y(myYcoord + 1), myZcoord), TOP,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#if 0
  for(int k=0; k<blockDimZ; ++k)
    for(int i=0; i<blockDimX; ++i) {
      temperature[index(i+1, blockDimY+1, k+1)] = top_plane_in[k*blockDimX+i];
    }
#endif
  int k, j, i;
  j = blockDimY;
  for (k = 1; k < blockDimZ + 1; k++)
    for (i = 1; i < blockDimX + 1; i++) {
      new_temperature[index(i, j, k)] =
          (temperature[index(i - 1, j, k)] + temperature[index(i + 1, j, k)] +
           temperature[index(i, j - 1, k)] +
           top_plane_in[(k - 1) * blockDimX + (i - 1)] +
           temperature[index(i, j, k - 1)] + temperature[index(i, j, k + 1)] +
           temperature[index(i, j, k)]) *
          DIVIDEBY7;
    }
}

void down(intptr_t) {
  MPI_Recv(bottom_plane_in, blockDimX * blockDimZ, MPI_DOUBLE,
            calc_pe(myXcoord, wrap_y(myYcoord - 1), myZcoord), BOTTOM,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#if 0
  for(int k=0; k<blockDimZ; ++k)
    for(int i=0; i<blockDimX; ++i) {
      temperature[index(i+1, 0, k+1)] = bottom_plane_in[k*blockDimX+i];
    }
#endif
  int k, j, i;
  j = 1;
  for (k = 1; k < blockDimZ + 1; k++)
    for (i = 1; i < blockDimX + 1; i++) {
      new_temperature[index(i, j, k)] =
          (temperature[index(i - 1, j, k)] + temperature[index(i + 1, j, k)] +
           bottom_plane_in[(k - 1) * blockDimX + (i - 1)] +
           temperature[index(i, j + 1, k)] + temperature[index(i, j, k - 1)] +
           temperature[index(i, j, k + 1)] + temperature[index(i, j, k)]) *
          DIVIDEBY7;
    }
}

void front(intptr_t) {
  MPI_Recv(front_plane_in, blockDimX * blockDimY, MPI_DOUBLE,
            calc_pe(myXcoord, myYcoord, wrap_z(myZcoord + 1)), FRONT,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#if 0
  for(int j=0; j<blockDimY; ++j)
    for(int i=0; i<blockDimX; ++i) {
      temperature[index(i+1, j+1, blockDimY+1)] = front_plane_in[j*blockDimX+i];
    }
#endif
  int k, j, i;
  k = blockDimY;
  for (j = 1; j < blockDimY + 1; j++)
    for (i = 1; i < blockDimX + 1; i++) {
      new_temperature[index(i, j, k)] =
          (temperature[index(i - 1, j, k)] + temperature[index(i + 1, j, k)] +
           temperature[index(i, j - 1, k)] + temperature[index(i, j + 1, k)] +
           temperature[index(i, j, k - 1)] +
           front_plane_in[(j - 1) * blockDimX + (i - 1)] +
           temperature[index(i, j, k)]) *
          DIVIDEBY7;
    }
}

void back(intptr_t) {
  MPI_Recv(back_plane_in, blockDimX * blockDimY, MPI_DOUBLE,
            calc_pe(myXcoord, myYcoord, wrap_z(myZcoord - 1)), BACK,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#if 0
  for(int j=0; j<blockDimY; ++j)
    for(int i=0; i<blockDimX; ++i) {
      temperature[index(i+1, j+1, 0)] = back_plane_in[j*blockDimX+i];
    }
#endif
  int j, k, i;
  k = 1;
  for (j = 1; j < blockDimY + 1; j++)
    for (i = 1; i < blockDimX + 1; i++) {
      new_temperature[index(i, j, k)] =
          (temperature[index(i - 1, j, k)] + temperature[index(i + 1, j, k)] +
           temperature[index(i, j - 1, k)] + temperature[index(i, j + 1, k)] +
           back_plane_in[(j - 1) * blockDimX + (i - 1)] +
           temperature[index(i, j, k + 1)] + temperature[index(i, j, k)]) *
          DIVIDEBY7;
    }
}

void send_left(intptr_t) {
    int k, j;
    for (k = 0; k < blockDimZ; ++k)
      for (j = 0; j < blockDimY; ++j) {
        left_plane_out[k * blockDimY + j] = temperature[index(1, j + 1, k + 1)];
      }
    MPI_Send(left_plane_out, blockDimY * blockDimZ, MPI_DOUBLE,
              calc_pe(wrap_x(myXcoord - 1), myYcoord, myZcoord), RIGHT,
              MPI_COMM_WORLD);
}

void send_right(intptr_t) {
    int k, j;
    for (k = 0; k < blockDimZ; ++k)
      for (j = 0; j < blockDimY; ++j) {
        right_plane_out[k * blockDimY + j] = temperature[index(blockDimX, j + 1, k + 1)];
      }
    MPI_Send(right_plane_out, blockDimY * blockDimZ, MPI_DOUBLE,
              calc_pe(wrap_x(myXcoord + 1), myYcoord, myZcoord), LEFT,
              MPI_COMM_WORLD);
}

void send_bot(intptr_t) {
    int k, i;
    for (k = 0; k < blockDimZ; ++k)
      for (i = 0; i < blockDimX; ++i) {
        bottom_plane_out[k * blockDimX + i] = temperature[index(i + 1, blockDimY, k + 1)];
      }
    MPI_Send(bottom_plane_out, blockDimX * blockDimZ, MPI_DOUBLE,
              calc_pe(myXcoord, wrap_y(myYcoord - 1), myZcoord), TOP,
              MPI_COMM_WORLD);
}

void send_top(intptr_t) {
    int k, i;
    for (k = 0; k < blockDimZ; ++k)
      for (i = 0; i < blockDimX; ++i) {
        top_plane_out[k * blockDimX + i] = temperature[index(i + 1, 1, k + 1)];
      }
    MPI_Send(top_plane_out, blockDimX * blockDimZ, MPI_DOUBLE,
              calc_pe(myXcoord, wrap_y(myYcoord + 1), myZcoord), BOTTOM,
              MPI_COMM_WORLD);
}

void send_back(intptr_t) {
    int j, i;
    for (j = 0; j < blockDimY; ++j)
      for (i = 0; i < blockDimX; ++i) {
        back_plane_out[j * blockDimX + i] = temperature[index(i + 1, j + 1, 1)];
      }
    MPI_Send(back_plane_out, blockDimX * blockDimY, MPI_DOUBLE,
              calc_pe(myXcoord, myYcoord, wrap_z(myZcoord - 1)), FRONT,
              MPI_COMM_WORLD);
}

void send_front(intptr_t) {
    int j, i;
    for (j = 0; j < blockDimY; ++j)
      for (i = 0; i < blockDimX; ++i) {
        front_plane_out[j * blockDimX + i] =
            temperature[index(i + 1, j + 1, blockDimZ)];
      }
    MPI_Send(front_plane_out, blockDimX * blockDimY, MPI_DOUBLE,
              calc_pe(myXcoord, myYcoord, wrap_z(myZcoord + 1)), BACK,
              MPI_COMM_WORLD);
}

int mpiv_work_start, mpiv_work_end;

int PER_THREAD = 64*8;

void compute(intptr_t k) {

#if USE_MPE
  if (wid == 0)
    MPE_Log_event(mpiv_work_start, 0, "work");
#endif
    for(int j=2; j<blockDimY; j++)
      for(int i=2; i<blockDimX; i++) {
        new_temperature[index(i,j,k)] =
          (temperature[index(i - 1, j, k)] + temperature[index(i + 1, j, k)] +
           temperature[index(i, j - 1, k)] + temperature[index(i, j + 1, k)] +
           temperature[index(i, j, k - 1)] + temperature[index(i, j, k + 1)] +
           temperature[index(i, j, k)]) *
          DIVIDEBY7;
      }
#if USE_MPE
  if (wid == 0)
    MPE_Log_event(mpiv_work_end, 0, "work");
#endif
}

void main_task(intptr_t) {
#if USE_MPE
  mpiv_work_start = MPE_Log_get_event_number(); 
  mpiv_work_end = MPE_Log_get_event_number(); 
  MPE_Describe_state(mpiv_work_start, mpiv_work_end, "WORK", "yellow");
#endif

  num_blocks_x = arrayDimX / blockDimX;
  num_blocks_y = arrayDimY / blockDimY;
  num_blocks_z = arrayDimZ / blockDimZ;

  if (myRank == 0) {
    printf("Running Jacobi on %d processors with (%d, %d, %d) elements\n",
           numPes, num_blocks_x, num_blocks_y, num_blocks_z);
    printf("Array Dimensions: %d %d %d\n", arrayDimX, arrayDimY, arrayDimZ);
    printf("Block Dimensions: %d %d %d\n", blockDimX, blockDimY, blockDimZ);
  }

  myXcoord = myRank % num_blocks_x;
  myYcoord = (myRank % (num_blocks_x * num_blocks_y)) / num_blocks_x;
  myZcoord = myRank / (num_blocks_x * num_blocks_y);

  int iterations = 0, i, j, k;
  double error = 1.0, max_error = 0.0;

  /* allocate one dimensional arrays */
  temperature = new double[(blockDimX + 2) * (blockDimY + 2) * (blockDimZ + 2)];
  new_temperature =
      new double[(blockDimX + 2) * (blockDimY + 2) * (blockDimZ + 2)];

  for (k = 0; k < blockDimZ + 2; k++)
    for (j = 0; j < blockDimY + 2; j++)
      for (i = 0; i < blockDimX + 2; i++) {
        temperature[index(i, j, k)] = 0.0;
        //printf("%d %d %d %d\n", i, j, k, index(i,j,k));
        //printf("i: %d\n", (index(i,j,k) % ((blockDimX + 2) * (blockDimY + 2))) % (blockDimX + 2));
        //printf("j: %d\n", (index(i,j,k) % ((blockDimX + 2) * (blockDimY + 2))) / (blockDimX + 2));
        //printf("k: %d\n", (index(i,j,k) / ((blockDimX + 2) * (blockDimY + 2))));
      }

  /* boundary conditions */
  if (myZcoord == 0 && myYcoord < num_blocks_y / 2 &&
      myXcoord < num_blocks_x / 2) {
    for (j = 1; j <= blockDimY; j++)
      for (i = 1; i <= blockDimX; i++) temperature[index(i, j, 1)] = 1.0;
  }

  if (myZcoord == num_blocks_z - 1 && myYcoord >= num_blocks_y / 2 &&
      myXcoord >= num_blocks_x / 2) {
    for (j = 1; j <= blockDimY; j++)
      for (i = 1; i <= blockDimX; i++)
        temperature[index(i, j, blockDimZ)] = 0.0;
  }

  /* Copy left, right, bottom, top, front and back  planes into temporary
   * arrays. */

  left_plane_out = (double*)malloc(sizeof(double) * blockDimY * blockDimZ);
  right_plane_out =
      (double*)malloc(sizeof(double) * blockDimY * blockDimZ);
  left_plane_in = (double*)malloc(sizeof(double) * blockDimY * blockDimZ);
  right_plane_in = (double*)malloc(sizeof(double) * blockDimY * blockDimZ);

  bottom_plane_out =
      (double*)malloc(sizeof(double) * blockDimX * blockDimZ);
  top_plane_out = (double*)malloc(sizeof(double) * blockDimX * blockDimZ);
  bottom_plane_in =
      (double*)malloc(sizeof(double) * blockDimX * blockDimZ);
  top_plane_in = (double*)malloc(sizeof(double) * blockDimX * blockDimZ);

  back_plane_out = (double*)malloc(sizeof(double) * blockDimX * blockDimY);
  front_plane_out =
      (double*)malloc(sizeof(double) * blockDimX * blockDimY);
  back_plane_in = (double*)malloc(sizeof(double) * blockDimX * blockDimY);
  front_plane_in = (double*)malloc(sizeof(double) * blockDimX * blockDimY);

  std::vector<thread> x;
  std::vector<thread> extra;
  x.reserve(blockDimZ);

  printf("starting...%d %d %d\n", myXcoord, myYcoord, myZcoord);

  int wcom = 0;

  while (/*error > 0.001 &&*/ iterations < MAX_ITER) {
    iterations++;
    if (iterations == SKIP_ITER) startTime = MPI_Wtime();

    /* Receive my right, left, top, bottom, back and front planes */
    auto r = MPIV_spawn(1, right, 0);
    auto l = MPIV_spawn(2, left, 0);
    auto u = MPIV_spawn(3, up, 0);
    auto d = MPIV_spawn(4, down, 0);
    auto f = MPIV_spawn(5, front, 0);
    auto b = MPIV_spawn(6, back, 0);

    /* Send data. */
    auto sr = MPIV_spawn(7, send_right, 0);
    auto sl = MPIV_spawn(8, send_left, 0);
    auto su = MPIV_spawn(9, send_top, 0);
    auto sd = MPIV_spawn(10, send_bot, 0);
    auto sf = MPIV_spawn(11, send_front, 0);
    auto sb = MPIV_spawn(12, send_back, 0);

    x.clear();
    for (int k=2; k<blockDimZ; k++) {
      // int w = ((x.size() % (NWORKER-1)) + 1);
      int w = x.size() % NWORKER;
      x.push_back(MPIV_spawn(w, compute, k));
    }

    // finish computation.
    for (auto& xi : x) {
      MPIV_join(xi);
    }

    // finish send.
    MPIV_join(sr);
    MPIV_join(sl);
    MPIV_join(su);
    MPIV_join(sd);
    MPIV_join(sf);
    MPIV_join(sb);

    // finish recv.
    MPIV_join(r);
    MPIV_join(l);
    MPIV_join(u);
    MPIV_join(d);
    MPIV_join(f);
    MPIV_join(b);

#if 0
    max_error = error = 0.0;
    for(k=1; k<blockDimZ+1; k++)
      for(j=1; j<blockDimY+1; j++)
        for(i=1; i<blockDimX+1; i++) {
          error = fabs(new_temperature[index(i, j, k)] - temperature[index(i, j, k)]);
          if(error > max_error)
            max_error = error;
        }
#endif

    double* tmp;
    tmp = temperature;
    temperature = new_temperature;
    new_temperature = tmp;

    /* boundary conditions */
    if (myZcoord == 0 && myYcoord < num_blocks_y / 2 &&
        myXcoord < num_blocks_x / 2) {
      for (j = 1; j <= blockDimY; j++)
        for (i = 1; i <= blockDimX; i++) temperature[index(i, j, 1)] = 1.0;
    }

    if (myZcoord == num_blocks_z - 1 && myYcoord >= num_blocks_y / 2 &&
        myXcoord >= num_blocks_x / 2) {
      for (j = 1; j <= blockDimY; j++)
        for (i = 1; i <= blockDimX; i++)
          temperature[index(i, j, blockDimZ)] = 0.0;
    }

    // if(myRank == 0) printf("Iteration %d\n", iterations);
    if (noBarrier == 0) MPI_Barrier(MPI_COMM_WORLD);
    // printf("%d %d done barrier\n", iterations, myRank);
    // MPI_Allreduce(&max_error, &error, 1, MPI_DOUBLE, MPI_MAX,
    // MPI_COMM_WORLD);
  } /* end of while loop */

  if (myRank == 0) {
    endTime = MPI_Wtime();
    printf("Completed %d iterations\n", iterations);
    printf("Time elapsed per iteration: %f\n",
           (endTime - startTime) / (MAX_ITER - SKIP_ITER));
  }
} /* end function main */
