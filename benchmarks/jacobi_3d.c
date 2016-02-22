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

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>

/* We want to wrap entries around, and because mod operator % sometimes
 * misbehaves on negative values. -1 maps to the highest value.
 */
#define wrap_x(a)	(((a)+num_blocks_x)%num_blocks_x)
#define wrap_y(a)	(((a)+num_blocks_y)%num_blocks_y)
#define wrap_z(a)	(((a)+num_blocks_z)%num_blocks_z)

#define index(a,b,c)	((a)+(b)*(blockDimX+2)+(c)*(blockDimX+2)*(blockDimY+2))
#define calc_pe(a,b,c)	((a)+(b)*num_blocks_x+(c)*num_blocks_x*num_blocks_y)

#define MAX_ITER	25
#define LEFT		1
#define RIGHT		2
#define TOP		3
#define BOTTOM		4
#define FRONT		5
#define BACK		6
#define DIVIDEBY7	0.14285714285714285714

double startTime;
double endTime;

int main(int argc, char **argv) {
  int myRank, numPes;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numPes);
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Request req[6];
  MPI_Status status[6];

  int blockDimX, blockDimY, blockDimZ;
  int arrayDimX, arrayDimY, arrayDimZ;
  int noBarrier = 0;

  if (argc != 4 && argc != 8) {
    printf("%s [array_size] [block_size] +[no]barrier\n", argv[0]);
    printf("%s [array_size_X] [array_size_Y] [array_size_Z] [block_size_X] [block_size_Y] [block_size_Z] +[no]barrier\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, -1);
  }

  if(argc == 4) {
    arrayDimZ = arrayDimY = arrayDimX = atoi(argv[1]);
    blockDimZ = blockDimY = blockDimX = atoi(argv[2]);
    if(strcasecmp(argv[3], "+nobarrier") == 0)
      noBarrier = 1;
    else
      noBarrier = 0;
    if(noBarrier && myRank==0) printf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
  }
  else {
    arrayDimX = atoi(argv[1]);
    arrayDimY = atoi(argv[2]);
    arrayDimZ = atoi(argv[3]);
    blockDimX = atoi(argv[4]);
    blockDimY = atoi(argv[5]);
    blockDimZ = atoi(argv[6]);
    if(strcasecmp(argv[7], "+nobarrier") == 0)
      noBarrier = 1;
    else
      noBarrier = 0;
    if(noBarrier && myRank==0) printf("\nSTENCIL COMPUTATION WITH NO BARRIERS\n");
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

  int num_blocks_x = arrayDimX / blockDimX;
  int num_blocks_y = arrayDimY / blockDimY;
  int num_blocks_z = arrayDimZ / blockDimZ;

  int myXcoord = myRank % num_blocks_x;
  int myYcoord = (myRank % (num_blocks_x * num_blocks_y)) / num_blocks_x;
  int myZcoord = myRank / (num_blocks_x * num_blocks_y);

  int iterations = 0, i, j, k;
  double error = 1.0, max_error = 0.0;

  if(myRank == 0) {
    printf("Running Jacobi on %d processors with (%d, %d, %d) elements\n", numPes, num_blocks_x, num_blocks_y, num_blocks_z);
    printf("Array Dimensions: %d %d %d\n", arrayDimX, arrayDimY, arrayDimZ);
    printf("Block Dimensions: %d %d %d\n", blockDimX, blockDimY, blockDimZ);
  }

  double *temperature;
  double *new_temperature;

  /* allocate one dimensional arrays */
  temperature = new double[(blockDimX+2) * (blockDimY+2) * (blockDimZ+2)];
  new_temperature = new double[(blockDimX+2) * (blockDimY+2) * (blockDimZ+2)];

  for(k=0; k<blockDimZ+2; k++)
    for(j=0; j<blockDimY+2; j++)
      for(i=0; i<blockDimX+2; i++) {
	temperature[index(i, j, k)] = 0.0;
      }

  /* boundary conditions */
  if(myZcoord == 0 && myYcoord < num_blocks_y/2 && myXcoord < num_blocks_x/2) {
    for(j=1; j<=blockDimY; j++)
      for(i=1; i<=blockDimX; i++)
	temperature[index(i, j, 1)] = 1.0;
  }

  if(myZcoord == num_blocks_z-1 && myYcoord >= num_blocks_y/2 && myXcoord >= num_blocks_x/2) {
    for(j=1; j<=blockDimY; j++)
      for(i=1; i<=blockDimX; i++)
      temperature[index(i, j, blockDimZ)] = 0.0;
  }

  /* Copy left, right, bottom, top, front and back  planes into temporary arrays. */

  double *left_plane_out   = new double[blockDimY*blockDimZ];
  double *right_plane_out  = new double[blockDimY*blockDimZ];
  double *left_plane_in    = new double[blockDimY*blockDimZ];
  double *right_plane_in   = new double[blockDimY*blockDimZ];

  double *bottom_plane_out = new double[blockDimX*blockDimZ];
  double *top_plane_out	   = new double[blockDimX*blockDimZ];
  double *bottom_plane_in  = new double[blockDimX*blockDimZ];
  double *top_plane_in     = new double[blockDimX*blockDimZ];

  double *back_plane_out    = new double[blockDimX*blockDimY];
  double *front_plane_out   = new double[blockDimX*blockDimY];
  double *back_plane_in     = new double[blockDimX*blockDimY];
  double *front_plane_in    = new double[blockDimX*blockDimY];

  while(/*error > 0.001 &&*/ iterations < MAX_ITER) {
    iterations++;
    if(iterations == 5) startTime = MPI_Wtime();

    /* Copy different planes into buffers */
    for(k=0; k<blockDimZ; ++k)
      for(j=0; j<blockDimY; ++j) {
        left_plane_out[k*blockDimY+j] = temperature[index(1, j+1, k+1)];
        right_plane_out[k*blockDimY+j] = temperature[index(blockDimX, j+1, k+1)];
      }

    for(k=0; k<blockDimZ; ++k)
      for(i=0; i<blockDimX; ++i) {
        top_plane_out[k*blockDimX+i] = temperature[index(i+1, 1, k+1)];
        bottom_plane_out[k*blockDimX+i] = temperature[index(i+1, blockDimY, k+1)];
      }

    for(j=0; j<blockDimY; ++j)
      for(i=0; i<blockDimX; ++i) {
        back_plane_out[j*blockDimX+i] = temperature[index(i+1, j+1, 1)];
        front_plane_out[j*blockDimX+i] = temperature[index(i+1, j+1, blockDimZ)];
      }

    /* Receive my right, left, top, bottom, back and front planes */
    MPI_Irecv(right_plane_in, blockDimY*blockDimZ, MPI_DOUBLE, calc_pe(wrap_x(myXcoord+1), myYcoord, myZcoord), RIGHT, MPI_COMM_WORLD, &req[RIGHT-1]);
    MPI_Irecv(left_plane_in, blockDimY*blockDimZ, MPI_DOUBLE, calc_pe(wrap_x(myXcoord-1), myYcoord, myZcoord), LEFT, MPI_COMM_WORLD, &req[LEFT-1]);
    MPI_Irecv(top_plane_in, blockDimX*blockDimZ, MPI_DOUBLE, calc_pe(myXcoord, wrap_y(myYcoord+1), myZcoord), TOP, MPI_COMM_WORLD, &req[TOP-1]);
    MPI_Irecv(bottom_plane_in, blockDimX*blockDimZ, MPI_DOUBLE, calc_pe(myXcoord, wrap_y(myYcoord-1), myZcoord), BOTTOM, MPI_COMM_WORLD, &req[BOTTOM-1]);
    MPI_Irecv(front_plane_in, blockDimX*blockDimY, MPI_DOUBLE, calc_pe(myXcoord, myYcoord, wrap_z(myZcoord+1)), FRONT, MPI_COMM_WORLD, &req[FRONT-1]);
    MPI_Irecv(back_plane_in, blockDimX*blockDimY, MPI_DOUBLE, calc_pe(myXcoord, myYcoord, wrap_z(myZcoord-1)), BACK, MPI_COMM_WORLD, &req[BACK-1]);


    /* Send my left, right, bottom, top, front and back planes */
    MPI_Send(left_plane_out, blockDimY*blockDimZ, MPI_DOUBLE, calc_pe(wrap_x(myXcoord-1), myYcoord, myZcoord), RIGHT, MPI_COMM_WORLD);
    MPI_Send(right_plane_out, blockDimY*blockDimZ, MPI_DOUBLE, calc_pe(wrap_x(myXcoord+1), myYcoord, myZcoord), LEFT, MPI_COMM_WORLD);
    MPI_Send(bottom_plane_out, blockDimX*blockDimZ, MPI_DOUBLE, calc_pe(myXcoord, wrap_y(myYcoord-1), myZcoord), TOP, MPI_COMM_WORLD);
    MPI_Send(top_plane_out, blockDimX*blockDimZ, MPI_DOUBLE, calc_pe(myXcoord, wrap_y(myYcoord+1), myZcoord), BOTTOM, MPI_COMM_WORLD);
    MPI_Send(back_plane_out, blockDimX*blockDimY, MPI_DOUBLE, calc_pe(myXcoord, myYcoord, wrap_z(myZcoord-1)), FRONT, MPI_COMM_WORLD);
    MPI_Send(front_plane_out, blockDimX*blockDimY, MPI_DOUBLE, calc_pe(myXcoord, myYcoord, wrap_z(myZcoord+1)), BACK, MPI_COMM_WORLD);

    MPI_Waitall(6, req, status);

    /* Copy buffers into ghost layers */
    for(k=0; k<blockDimZ; ++k)
      for(j=0; j<blockDimY; ++j) {
	temperature[index(0, j+1, k+1)] = left_plane_in[k*blockDimY+j];
      }
    for(k=0; k<blockDimZ; ++k)
      for(j=0; j<blockDimY; ++j) {
	temperature[index(blockDimX+1, j+1, k+1)] = right_plane_in[k*blockDimY+j];
      }
    for(k=0; k<blockDimZ; ++k)
      for(i=0; i<blockDimX; ++i) {
	temperature[index(i+1, 0, k+1)] = bottom_plane_in[k*blockDimX+i];
      }
    for(k=0; k<blockDimZ; ++k)
      for(i=0; i<blockDimX; ++i) {
	temperature[index(i+1, blockDimY+1, k+1)] = top_plane_in[k*blockDimX+i];
      }
    for(j=0; j<blockDimY; ++j)
      for(i=0; i<blockDimX; ++i) {
	temperature[index(i+1, j+1, 0)] = back_plane_in[j*blockDimX+i];
      }
    for(j=0; j<blockDimY; ++j)
      for(i=0; i<blockDimX; ++i) {
	temperature[index(i+1, j+1, blockDimY+1)] = top_plane_in[j*blockDimX+i];
      }

    /* update my value based on the surrounding values */
    for(k=1; k<blockDimZ+1; k++)
      for(j=1; j<blockDimY+1; j++)
	for(i=1; i<blockDimX+1; i++) {
	  new_temperature[index(i, j, k)] = (temperature[index(i-1, j, k)]
                                          +  temperature[index(i+1, j, k)]
                                          +  temperature[index(i, j-1, k)]
                                          +  temperature[index(i, j+1, k)]
                                          +  temperature[index(i, j, k-1)]
                                          +  temperature[index(i, j, k+1)]
                                          +  temperature[index(i, j, k)] ) * DIVIDEBY7;
	}

    max_error = error = 0.0;
    for(k=1; k<blockDimZ+1; k++)
      for(j=1; j<blockDimY+1; j++)
	for(i=1; i<blockDimX+1; i++) {
	  error = fabs(new_temperature[index(i, j, k)] - temperature[index(i, j, k)]);
	  if(error > max_error)
	    max_error = error;
	}
 
    double *tmp;
    tmp = temperature;
    temperature = new_temperature;
    new_temperature = tmp;

    /* boundary conditions */
    if(myZcoord == 0 && myYcoord < num_blocks_y/2 && myXcoord < num_blocks_x/2) {
      for(j=1; j<=blockDimY; j++)
	for(i=1; i<=blockDimX; i++)
	  temperature[index(i, j, 1)] = 1.0;
    }

    if(myZcoord == num_blocks_z-1 && myYcoord >= num_blocks_y/2 && myXcoord >= num_blocks_x/2) {
      for(j=1; j<=blockDimY; j++)
	for(i=1; i<=blockDimX; i++)
	temperature[index(i, j, blockDimZ)] = 0.0;
    }

    // if(myRank == 0) printf("Iteration %d %f\n", iterations, max_error);
    if(noBarrier == 0) MPI_Allreduce(&max_error, &error, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  } /* end of while loop */

  if(myRank == 0) {
    endTime = MPI_Wtime();
    printf("Completed %d iterations\n", iterations);
    printf("Time elapsed per iteration: %f\n", (endTime - startTime)/(MAX_ITER-5));
  }

  MPI_Finalize();
  return 0;
} /* end function main */

