#ifndef ULT_H_
#define ULT_H_

#include <boost/function.hpp>
typedef boost::function<void(intptr_t)> ffunc;
static const int F_STACK_SIZE = 4*1024;
static const int MAIN_STACK_SIZE = 16 * 1024;

class ult_base {
 public:
  virtual void yield() = 0;
  virtual void wait() = 0;
  virtual void resume() = 0;
  virtual void join() = 0;

  virtual void start() = 0;
  virtual void done() = 0;
};

#include "fult.h"

using thread = fult;
using worker = fworker;

#endif
