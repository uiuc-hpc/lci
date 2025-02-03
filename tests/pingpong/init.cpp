#include "lci.hpp"

int main(int argc, char** args)
{
  lci::backend::domain_t domain;
  domain.alloc();
  domain.free();
  return 0;
}
