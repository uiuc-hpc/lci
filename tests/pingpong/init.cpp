#include "lcixx.hpp"

int main(int argc, char** args)
{
  lcixx::backend::domain_t domain;
  domain.alloc();
  domain.free();
  return 0;
}
