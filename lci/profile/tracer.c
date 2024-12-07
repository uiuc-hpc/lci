#include "runtime/lcii.h"

LCT_tracer_t LCII_tracer;

void LCII_tracer_init()
{
#ifdef LCI_USE_TRACER
  const char *filename = getenv("LCI_TRACER_FILENAME");
  if (!filename) {
    filename = "lci_%.trace";
  }
  bool write_binary = LCIU_getenv_or("LCI_TRACER_WRITE_BINARY", true);
  LCII_tracer = LCT_tracer_init("lci", (const char*[]){"net"}, 1, filename, write_binary);
#endif  // LCI_USE_TRACER
}

void LCII_tracer_fina()
{
#ifdef LCI_USE_TRACER
  LCT_tracer_fina(LCII_tracer);
#endif  // LCI_USE_TRACER
}