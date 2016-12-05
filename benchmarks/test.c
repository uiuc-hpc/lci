#include <mpi.h>

int main(int argc, char** args) {
  int provided;
  MPI_Init_thread(&argc, &args, MPI_THREAD_MULTIPLE, &provided);
  if (provided != MPI_THREAD_MULTIPLE) {
    MPI_Abort(MPI_COMM_WORLD, 0);
  }
  MPI_Finalize();
}
