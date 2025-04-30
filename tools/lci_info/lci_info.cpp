#include <iostream>
#include "lci.hpp"
#include "lci_info_cache.hpp"

void print_help() {
  std::cout << "Usage: lci_info [options]\n";
  std::cout << "Options:\n";
  std::cout << "  -v, --verbose    Enable verbose output\n";
}

int main(int argc, char** argv) {
  bool verbose = false;
  if (argc == 2) {
    if (std::string(argv[1]) == "-v" || std::string(argv[1]) == "--verbose") {
      verbose = true;
    } else {
      std::cerr << "Unknown option: " << argv[1] << "\n";
      print_help();
      return 1;
    }
  } else if (argc > 2) {
    std::cerr << "Too many arguments\n";
    print_help();
    return 1;
  }

  std::cout << "LCI Library Information\n";
  std::cout << "------------------------\n";
  std::cout << "Version           : " << LCI_VERSION_STRING << "\n";
  std::cout << "Version Full      : " << LCI_VERSION_FULL << "\n";
  std::cout << "Build Timestamp   : " << LCI_BUILD_TIMESTAMP << "\n";
  std::cout << "Backends Enabled  : " << LCI_NETWORK_BACKENDS_ENABLED << "\n";
  if (verbose) {
    std::cout << "------------------------\n";
    std::cout << "CMake Configuration: \n";
    std::cout << LCI_CMAKE_CACHE_INFO;
    std::cout << "------------------------\n";
  }
  return 0;
}