#ifndef MPIV_MPI_H_
#define MPIV_MPI_H_

extern int worker_id();

namespace mpiv {

#include "recv.h"
#include "irecv.h"
#include "send.h"
#include "waitall.h"
#include "coll/collective.h"

}; // namespace mpiv.

#endif
