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

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>

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

int main(int argc, char** argv)
{
  int myRank, numPes;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numPes);
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Request req[4];
  MPI_Status status[4];

  int blockDimX, blockDimY, arrayDimX, arrayDimY;
  int noBarrier = 0;

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
    printf("array_size_X % block_size_X != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }
  if (arrayDimY < blockDimY || arrayDimY % blockDimY != 0) {
    printf("array_size_Y % block_size_Y != 0!\n");
    MPI_Abort(MPI_COMM_WORLD, -1);
  }

  int num_blocks_x = arrayDimX / blockDimX;
  int num_blocks_y = arrayDimY / blockDimY;

  int myRow = myRank / num_blocks_y;
  int myCol = myRank % num_blocks_y;
  int iterations = 0, i, j;
  double error = 1.0, max_error = 0.0;

  if (myRank == 0) {
    printf("Running Jacobi on %d processors with (%d, %d) elements\n", numPes,
           num_blocks_x, num_blocks_y);
    printf("Array Dimensions: %d %d\n", arrayDimX, arrayDimY);
    printf("Block Dimensions: %d %d\n", blockDimX, blockDimY);
  }

  double** temperature;
  double** new_temperature;

  /* allocate two dimensional arrays */
  temperature = new double*[blockDimX + 2];
  new_temperature = new double*[blockDimX + 2];
  for (i = 0; i < blockDimX + 2; i++) {
    temperature[i] = new double[blockDimY + 2];
    new_temperature[i] = new double[blockDimY + 2];
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
  double* left_edge_out = new double[blockDimX];
  double* right_edge_out = new double[blockDimX];
  double* left_edge_in = new double[blockDimX];
  double* right_edge_in = new double[blockDimX];

  while (/*error > 0.001 &&*/ iterations < MAX_ITER) {
    iterations++;
    if (iterations == SKIP_ITER) startTime = MPI_Wtime();

    for (i = 0; i < blockDimX; i++) {
      left_edge_out[i] = temperature[i + 1][1];
      right_edge_out[i] = temperature[i + 1][blockDimY];
    }

    /* Receive my right, left, bottom and top edge */
    MPI_Irecv(right_edge_in, blockDimX, MPI_DOUBLE,
              calc_pe(myRow, wrap_y(myCol + 1)), RIGHT, MPI_COMM_WORLD,
              &req[RIGHT - 1]);
    MPI_Irecv(left_edge_in, blockDimX, MPI_DOUBLE,
              calc_pe(myRow, wrap_y(myCol - 1)), LEFT, MPI_COMM_WORLD,
              &req[LEFT - 1]);
    MPI_Irecv(&temperature[blockDimX + 1][1], blockDimY, MPI_DOUBLE,
              calc_pe(wrap_x(myRow + 1), myCol), BOTTOM, MPI_COMM_WORLD,
              &req[BOTTOM - 1]);
    MPI_Irecv(&temperature[0][1], blockDimY, MPI_DOUBLE,
              calc_pe(wrap_x(myRow - 1), myCol), TOP, MPI_COMM_WORLD,
              &req[TOP - 1]);

    /* Send my left, right, top and bottom edge */
    MPI_Send(left_edge_out, blockDimX, MPI_DOUBLE,
             calc_pe(myRow, wrap_y(myCol - 1)), RIGHT, MPI_COMM_WORLD);
    MPI_Send(right_edge_out, blockDimX, MPI_DOUBLE,
             calc_pe(myRow, wrap_y(myCol + 1)), LEFT, MPI_COMM_WORLD);
    MPI_Send(&temperature[1][1], blockDimY, MPI_DOUBLE,
             calc_pe(wrap_x(myRow - 1), myCol), BOTTOM, MPI_COMM_WORLD);
    MPI_Send(&temperature[blockDimX][1], blockDimY, MPI_DOUBLE,
             calc_pe(wrap_x(myRow + 1), myCol), TOP, MPI_COMM_WORLD);

    MPI_Waitall(4, req, status);

    for (i = 0; i < blockDimX; i++)
      temperature[i + 1][blockDimY + 1] = right_edge_in[i];
    for (i = 0; i < blockDimX; i++) temperature[i + 1][0] = left_edge_in[i];

    for (i = 1; i < blockDimX + 1; i++) {
      for (j = 1; j < blockDimY + 1; j++) {
        /* update my value based on the surrounding values */
        new_temperature[i][j] = (temperature[i - 1][j] + temperature[i + 1][j] +
                                 temperature[i][j - 1] + temperature[i][j + 1] +
                                 temperature[i][j]) *
                                0.2;
      }
    }

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

  MPI_Finalize();
  return 0;
} /* end function main */
