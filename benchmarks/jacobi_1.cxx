/** \file jacobi2d.C
 *  Author: Abhinav S Bhatele
 *  Date Created: February 19th, 2009
 *
 *
 *    ***********  ^
 *    *		*  |
 *    *		*  |
 *    *		*  X
 *    *		*  |
 *    *		*  |
 *    ***********  ~
 *    <--- Y --->
 *
 *    X: blockDimX, arrayDimX --> wrap_x
 *    Y: blockDimY, arrayDimY --> wrap_y
 *
 *  Two dimensional decomposition of a 2D stencil
 */

#include "mpiv.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

/* We want to wrap entries around, and because mod operator %
 * sometimes misbehaves on negative values. -1 maps to the highest value.*/
#define wrap_x(a) (((a) + num_blocks_x) % num_blocks_x)
#define wrap_y(a) (((a) + num_blocks_y) % num_blocks_y)
#define calc_pe(a, b) ((a)*num_blocks_y + (b))

#define MAX_ITER 500
#define SKIP_ITER 100
#define LEFT 1
#define RIGHT 2
#define TOP 3
#define BOTTOM 4

double startTime;
double endTime;

double* left_edge_out;
double* right_edge_out;
double* left_edge_in;
double* right_edge_in;
int blockDimX, blockDimY, arrayDimX, arrayDimY;
int myRank, numPes;
int myRow, myCol, num_blocks_x, num_blocks_y;
double** temperature;
double** new_temperature;

void recvL(intptr_t) {
  MPIV_Recv(left_edge_in, blockDimX * sizeof(double),
            calc_pe(myRow, wrap_y(myCol - 1)), LEFT);

  for (int i = 0; i < blockDimX; i++) temperature[i + 1][0] = left_edge_in[i];
  int j = 1;
  for (int i = 1; i < blockDimX + 1; i++) {
    new_temperature[i][j] =
        (temperature[i - 1][j] + temperature[i + 1][j] + temperature[i][j - 1] +
         temperature[i][j + 1] + temperature[i][j]) *
        0.2;
  }
}

void recvR(intptr_t) {
  MPIV_Recv(right_edge_in, blockDimX * sizeof(double),
            calc_pe(myRow, wrap_y(myCol + 1)), RIGHT);

  for (int i = 0; i < blockDimX; i++)
    temperature[i + 1][blockDimY + 1] = right_edge_in[i];
  int j = blockDimY;
  for (int i = 1; i < blockDimX + 1; i++) {
    new_temperature[i][j] =
        (temperature[i - 1][j] + temperature[i + 1][j] + temperature[i][j - 1] +
         temperature[i][j + 1] + temperature[i][j]) *
        0.2;
  }
}

void recvB(intptr_t) {
  MPIV_Recv(&temperature[blockDimX + 1][1], blockDimY * sizeof(double),
            calc_pe(wrap_x(myRow + 1), myCol), BOTTOM);
  for (int j = 1; j < blockDimY + 1; j++) {
    int i2 = blockDimX;
    new_temperature[i2][j] =
        (temperature[i2 - 1][j] + temperature[i2 + 1][j] +
         temperature[i2][j - 1] + temperature[i2][j + 1] + temperature[i2][j]) *
        0.2;
  }
}

void recvT(intptr_t) {
  MPIV_Recv(&temperature[0][1], blockDimY * sizeof(double),
            calc_pe(wrap_x(myRow - 1), myCol), TOP);
  for (int j = 1; j < blockDimY + 1; j++) {
    int i1 = 1;
    new_temperature[i1][j] =
        (temperature[i1 - 1][j] + temperature[i1 + 1][j] +
         temperature[i1][j - 1] + temperature[i1][j + 1] + temperature[i1][j]) *
        0.2;
  }
}

void sendL(intptr_t) {
  MPIV_Send(left_edge_out, blockDimX * sizeof(double),
            calc_pe(myRow, wrap_y(myCol - 1)), RIGHT);
}

void sendR(intptr_t) {
  MPIV_Send(right_edge_out, blockDimX * sizeof(double),
            calc_pe(myRow, wrap_y(myCol + 1)), LEFT);
}

void sendB(intptr_t) {
  MPIV_Send(&temperature[1][1], blockDimY * sizeof(double),
            calc_pe(wrap_x(myRow - 1), myCol), BOTTOM);
}

void sendT(intptr_t) {
  MPIV_Send(&temperature[blockDimX][1], blockDimY * sizeof(double),
            calc_pe(wrap_x(myRow + 1), myCol), TOP);
}

void compute(intptr_t) {
  int i, j;
  for (i = 2; i < blockDimX; i++) {
    for (j = 2; j < blockDimY; j++) {
      /* update my value based on the surrounding values */
      new_temperature[i][j] =
          (temperature[i - 1][j] + temperature[i + 1][j] +
           temperature[i][j - 1] + temperature[i][j + 1] + temperature[i][j]) *
          0.2;
    }
  }
}

int noBarrier = 0;

int main(int argc, char** argv) {
  MPIV_Init(argc, argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numPes);
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

  if (argc != 4 && argc != 6) {
    printf("%s [array_size] [block_size] +[no]barrier\n", argv[0]);
    printf(
        "%s [array_size_X] [array_size_Y] [block_size_X] [block_size_Y] "
        "+[no]barrier\n",
        argv[0]);
    MPI_Abort(MPI_COMM_WORLD, -1);
  }

  if (argc == 4) {
    arrayDimY = arrayDimX = atoi(argv[1]);
    blockDimY = blockDimX = atoi(argv[2]);
    if (strcasecmp(argv[3], "+nobarrier") == 0)
      noBarrier = 1;
    else
      noBarrier = 0;
    if (noBarrier && myRank == 0)
      printf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
  } else {
    arrayDimX = atoi(argv[1]);
    arrayDimY = atoi(argv[2]);
    blockDimX = atoi(argv[3]);
    blockDimY = atoi(argv[4]);
    if (strcasecmp(argv[5], "+nobarrier") == 0)
      noBarrier = 1;
    else
      noBarrier = 0;
    if (noBarrier && myRank == 0)
      printf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
  }

  if (arrayDimX < blockDimX || arrayDimX % blockDimX != 0) {
    printf("array_size_X %% block_size_X != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }
  if (arrayDimY < blockDimY || arrayDimY % blockDimY != 0) {
    printf("array_size_Y %% block_size_Y != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }

  num_blocks_x = arrayDimX / blockDimX;
  num_blocks_y = arrayDimY / blockDimY;

  myRow = myRank / num_blocks_y;
  myCol = myRank % num_blocks_y;
  if (myRank == 0) {
    printf("Running Jacobi on %d processors with (%d, %d) elements\n", numPes,
           num_blocks_x, num_blocks_y);
    printf("Array Dimensions: %d %d\n", arrayDimX, arrayDimY);
    printf("Block Dimensions: %d %d\n", blockDimX, blockDimY);
  }

  MPIV_Init_worker(2);
  MPIV_Finalize();

  return 0;
}

void main_task(intptr_t) {
  int iterations = 0, i, j;
  double error = 1.0, max_error = 0.0;

  /* allocate two dimensional arrays */
  temperature = new double*[blockDimX + 2];
  new_temperature = new double*[blockDimX + 2];
  for (i = 0; i < blockDimX + 2; i++) {
    temperature[i] = (double*)mpiv_malloc(sizeof(double) * (blockDimY + 2));
    new_temperature[i] = (double*)mpiv_malloc(sizeof(double) * (blockDimY + 2));
  }

  for (i = 0; i < blockDimX + 2; i++) {
    for (j = 0; j < blockDimY + 2; j++) {
      temperature[i][j] = 0.5;
      new_temperature[i][j] = 0.5;
    }
  }

  // boundary conditions
  if (myCol == 0 && myRow < num_blocks_x / 2) {
    for (i = 1; i <= blockDimX; i++) temperature[i][1] = 1.0;
  }

  if (myRow == num_blocks_x - 1 && myCol >= num_blocks_y / 2) {
    for (j = 1; j <= blockDimY; j++) temperature[blockDimX][j] = 0.0;
  }

  /* Copy left column and right column into temporary arrays */
  left_edge_out = (double*)mpiv_malloc(blockDimX * sizeof(double) + 2);
  right_edge_out = (double*)mpiv_malloc(blockDimX * sizeof(double) + 2);
  left_edge_in = (double*)mpiv_malloc(blockDimX * sizeof(double) + 2);
  right_edge_in = (double*)mpiv_malloc(blockDimX * sizeof(double) + 2);

  while (/*error > 0.001 &&*/ iterations < MAX_ITER) {
    iterations++;
    if (iterations == SKIP_ITER) startTime = MPI_Wtime();

    fult_t tcompute = MPIV_spawn(1, compute, 0);

    for (i = 0; i < blockDimX; i++) {
      left_edge_out[i] = temperature[i + 1][1];
      right_edge_out[i] = temperature[i + 1][blockDimY];
    }

    fult_t l = MPIV_spawn(0, recvL, 0);
    fult_t r = MPIV_spawn(0, recvR, 0);
    fult_t b = MPIV_spawn(0, recvB, 0);
    fult_t t = MPIV_spawn(0, recvT, 0);

    sendR(0);
    sendL(0);
    sendT(0);
    sendB(0);

    MPIV_join(0, l);
    MPIV_join(0, r);
    MPIV_join(0, b);
    MPIV_join(0, t);

    MPIV_join(1, tcompute);

    max_error = error = 0.0;
    for (i = 1; i < blockDimX + 1; i++) {
      for (j = 1; j < blockDimY + 1; j++) {
        error = fabs(new_temperature[i][j] - temperature[i][j]);
        if (error > max_error) max_error = error;
      }
    }

    double** tmp;
    tmp = temperature;
    temperature = new_temperature;
    new_temperature = tmp;

    // boundary conditions
    if (myCol == 0 && myRow < num_blocks_x / 2) {
      for (i = 1; i <= blockDimX; i++) temperature[i][1] = 1.0;
    }

    if (myRow == num_blocks_x - 1 && myCol >= num_blocks_y / 2) {
      for (j = 1; j <= blockDimY; j++) temperature[blockDimX][j] = 0.0;
    }

    // if(myRank == 0) printf("Iteration %d %f %f %f\n", iterations, max_error,
    // temperature[1][1], temperature[1][2]);
    if (noBarrier == 0)
      MPI_Allreduce(&max_error, &error, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  } /* end of while loop */

  if (myRank == 0) {
    endTime = MPI_Wtime();
    printf("Completed %d iterations\n", iterations);
    printf("Time elapsed per iteration: %f\n",
           (endTime - startTime) / (MAX_ITER - SKIP_ITER));
  }
} /* end function main */
