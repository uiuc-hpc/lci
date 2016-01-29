#ifndef EXCEPT_H_
#define EXCEPT_H_

#include <exception>
#include <string>
#include <iostream>

class hash_table_error : public std::exception {
 public:
  hash_table_error(const std::string& err_msg) {
    std::cerr << err_msg << std::endl;
  }
};
class packet_error : public std::exception {
 public:
  packet_error(const std::string& err_msg) {
    std::cerr << err_msg << std::endl;
  }
};

#endif
