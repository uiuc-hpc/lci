#define BENCHMARK "MPIV Multiple Bandwidth / Message Rate Test"
/*
 * Copyright (C) 2002-2014 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University. 
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include <mpiv.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define DEFAULT_WINDOW       (64)

#define ITERS_SMALL          (100)          
#define WARMUP_ITERS_SMALL   (10)
#define ITERS_LARGE          (20)
#define WARMUP_ITERS_LARGE   (2)
#define LARGE_THRESHOLD      (8192)

#define WINDOW_SIZES {8, 16, 32, 64, 128}
#define WINDOW_SIZES_COUNT   (5)

#define MAX_MSG_SIZE         (1<<22)
#define MAX_ALIGNMENT        (65536)
#define MY_BUF_SIZE (MAX_MSG_SIZE + MAX_ALIGNMENT)

// char s_buf1[MY_BUF_SIZE];
// char r_buf1[MY_BUF_SIZE];

MPIV_Request * request;
MPI_Status * reqstat;

double calc_bw(int rank, int size, int num_pairs, int window_size, char *s_buf, char *r_buf);
void usage();

#ifdef PACKAGE_VERSION
#   define HEADER "# " BENCHMARK " v" PACKAGE_VERSION "\n"
#else
#   define HEADER "# " BENCHMARK "\n"
#endif

#ifndef FIELD_WIDTH
#   define FIELD_WIDTH 20
#endif

#ifndef FLOAT_PRECISION
#   define FLOAT_PRECISION 2
#endif

char *s_buf, *r_buf;
int numprocs, rank, align_size;
int pairs, print_rate;
int window_size, window_varied;
int c, curr_size;

int main(int argc, char *argv[])
{
    MPIV_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* default values */
    pairs            = numprocs / 2;
    window_size      = DEFAULT_WINDOW;
    window_varied    = 0;
    print_rate       = 1;

    while((c = getopt(argc, argv, "p:w:r:vh")) != -1) {
        switch (c) {
            case 'p':
                pairs = atoi(optarg);

                if(pairs > (numprocs / 2)) {
                    if(0 == rank) {
                        usage();
                    }

                    goto error;
                }

                break;

            case 'w':
                window_size = atoi(optarg);
                break;

            case 'v':
                window_varied = 1;
                break;

            case 'r':
                print_rate = atoi(optarg);

                if(0 != print_rate && 1 != print_rate) {
                    if(0 == rank) {
                        usage();
                    }

                    goto error;
                }

                break;

            default:
                if(0 == rank) {
                    usage();
                }

                goto error;
        }
    }

    align_size = getpagesize();
    assert(align_size <= MAX_ALIGNMENT);

    s_buf = (char*) mpiv_malloc(MY_BUF_SIZE + align_size);
    r_buf = (char*) mpiv_malloc(MY_BUF_SIZE + align_size);

    if(numprocs < 2) {
        if(rank == 0) {
            fprintf(stderr, "This test requires at least two processes\n");
        }

        MPI_Finalize();

        return EXIT_FAILURE;
    }

    if(rank == 0) {
        fprintf(stdout, HEADER);

        if(window_varied) {
            fprintf(stdout, "# [ pairs: %d ] [ window size: varied ]\n", pairs);
            fprintf(stdout, "\n# Uni-directional Bandwidth (MB/sec)\n");
        }

        else {
            fprintf(stdout, "# [ pairs: %d ] [ window size: %d ]\n", pairs,
                    window_size);

            if(print_rate) {
                fprintf(stdout, "%-*s%*s%*s\n", 10, "# Size", FIELD_WIDTH,
                        "MB/s", FIELD_WIDTH, "Messages/s");
            }

            else {
                fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "MB/s");
            }
        }

        fflush(stdout);
    }

   /* More than one window size */
   MPIV_Init_worker(1);

error:
   MPIV_Finalize();
   return 0;
}

void main_task(intptr_t) {
   if(window_varied) {
       int window_array[] = WINDOW_SIZES;
       double ** bandwidth_results;
       int log_val = 1, tmp_message_size = MAX_MSG_SIZE;
       int i, j;

       for(i = 0; i < WINDOW_SIZES_COUNT; i++) {
           if(window_array[i] > window_size) {
               window_size = window_array[i];
           }
       }

       request = (MPIV_Request *) malloc(sizeof(MPI_Request) * window_size);
       reqstat = (MPI_Status *) malloc(sizeof(MPI_Status) * window_size);

       while(tmp_message_size >>= 1) {
           log_val++;
       }

       bandwidth_results = (double **) malloc(sizeof(double *) * log_val);

       for(i = 0; i < log_val; i++) {
           bandwidth_results[i] = (double *)malloc(sizeof(double) *
                   WINDOW_SIZES_COUNT);
       }

       if(rank == 0) {
           fprintf(stdout, "#      ");

           for(i = 0; i < WINDOW_SIZES_COUNT; i++) {
               fprintf(stdout, "  %10d", window_array[i]);
           }

           fprintf(stdout, "\n");
           fflush(stdout);
       }
    
       for(j = 0, curr_size = 1; curr_size <= MAX_MSG_SIZE; curr_size *= 2, j++) {
           if(rank == 0) {
               fprintf(stdout, "%-7d", curr_size);
           }

           for(i = 0; i < WINDOW_SIZES_COUNT; i++) {
               bandwidth_results[j][i] = calc_bw(rank, curr_size, pairs,
                       window_array[i], s_buf, r_buf);

               if(rank == 0) {
                   fprintf(stdout, "  %10.*f", FLOAT_PRECISION,
                           bandwidth_results[j][i]);
               }
           }

           if(rank == 0) {
               fprintf(stdout, "\n");
               fflush(stdout);
           }
       }

       if(rank == 0 && print_rate) {
            fprintf(stdout, "\n# Message Rate Profile\n");
            fprintf(stdout, "#      ");

            for(i = 0; i < WINDOW_SIZES_COUNT; i++) {
                fprintf(stdout, "  %10d", window_array[i]);
            }       

            fprintf(stdout, "\n");
            fflush(stdout);

            for(c = 0, curr_size = 1; curr_size <= MAX_MSG_SIZE; curr_size *= 2) { 
                fprintf(stdout, "%-7d", curr_size); 

                for(i = 0; i < WINDOW_SIZES_COUNT; i++) {
                    double rate = 1e6 * bandwidth_results[c][i] / curr_size;

                    fprintf(stdout, "  %10.2f", rate);
                }       

                fprintf(stdout, "\n");
                fflush(stdout);
                c++;    
            }
       }
   }

   else {
       /* Just one window size */
       request = (MPIV_Request *)malloc(sizeof(MPIV_Request) * window_size);
       reqstat = (MPI_Status *)malloc(sizeof(MPI_Status) * window_size);

       for(curr_size = 1; curr_size <= MAX_MSG_SIZE; curr_size *= 2) {
           double bw, rate;

           bw = calc_bw(rank, curr_size, pairs, window_size, s_buf, r_buf);

           if(rank == 0) {
               rate = 1e6 * bw / curr_size;

               if(print_rate) {
                   fprintf(stdout, "%-*d%*.*f%*.*f\n", 10, curr_size,
                           FIELD_WIDTH, FLOAT_PRECISION, bw, FIELD_WIDTH,
                           FLOAT_PRECISION, rate);
               }

               else {
                   fprintf(stdout, "%-*d%*.*f\n", 10, curr_size, FIELD_WIDTH,
                           FLOAT_PRECISION, bw);
               }
           } 
       }
   }
}

void usage() {
    printf("Options:\n");
    printf("  -r=<0,1>         Print uni-directional message rate (default 1)\n");
    printf("  -p=<pairs>       Number of pairs involved (default np / 2)\n");
    printf("  -w=<window>      Number of messages sent before acknowledgement (64, 10)\n");
    printf("                   [cannot be used with -v]\n");
    printf("  -v               Vary the window size (default no)\n");
    printf("                   [cannot be used with -w]\n");
    printf("  -h               Print this help\n");
    printf("\n");
    printf("  Note: This benchmark relies on block ordering of the ranks.  Please see\n");
    printf("        the README for more information.\n");
    fflush(stdout);
}

double calc_bw(int rank, int size, int num_pairs, int window_size, char *s_buf,
        char *r_buf)
{
    double t_start = 0, t_end = 0, t = 0, sum_time = 0, bw = 0;
    int i, j, target;
    int loop, skip;
    int mult = (DEFAULT_WINDOW / window_size) > 0 ? (DEFAULT_WINDOW /
            window_size) : 1;

    for(i = 0; i < size; i++) {
        s_buf[i] = 'a';
        r_buf[i] = 'b';
    }

    if(size > LARGE_THRESHOLD) {
        loop = ITERS_LARGE * mult;
        skip = WARMUP_ITERS_LARGE * mult;
    }

    else {
        loop = ITERS_SMALL * mult;
        skip = WARMUP_ITERS_SMALL * mult;
    }

    MPIV_Barrier(MPI_COMM_WORLD);

    if(rank < num_pairs) {
        target = rank + num_pairs;

        for(i = 0; i < loop + skip; i++) {
            if(i == skip) {
                MPIV_Barrier(MPI_COMM_WORLD);
                t_start = MPI_Wtime();
            }

            for(j = 0; j < window_size; j++) {
                MPIV_Isend(s_buf, size, MPI_CHAR, target, j, MPI_COMM_WORLD,
                        request + j);
            }

            MPIV_Waitall(window_size, request, reqstat);
            MPIV_Recv(r_buf, 4, MPI_CHAR, target, window_size + 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        t_end = MPI_Wtime();
        t = t_end - t_start;
    }

    else if(rank < num_pairs * 2) {
        target = rank - num_pairs;

        for(i = 0; i < loop + skip; i++) {
            if(i == skip) {
                MPIV_Barrier(MPI_COMM_WORLD);
            }

            for(j = 0; j < window_size; j++) {
                MPIV_Irecv(r_buf, size, MPI_CHAR, target, j, MPI_COMM_WORLD,
                        request + j);
            }

            MPIV_Waitall(window_size, request, reqstat);
            MPIV_Send(s_buf, 4, MPI_CHAR, target, window_size + 1, MPI_COMM_WORLD);
        }
    }

    else {
        MPIV_Barrier(MPI_COMM_WORLD);
    }

    MPIV_Barrier(MPI_COMM_WORLD);
    MPI_Reduce(&t, &sum_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if(rank == 0) {
        double tmp = size / 1e6 * num_pairs ;
        
        sum_time /= num_pairs;
        tmp = tmp * loop * window_size;
        bw = tmp / sum_time;

        return bw;
    }

    return 0;
}

/* vi: set sw=4 sts=4 tw=80: */
