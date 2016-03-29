#ifndef EXCEPT_H_
#define EXCEPT_H_

#include <exception>
#include <stdexcept>
#include <string>
#include <iostream>

class hash_table_error : public std::runtime_error {
 public:
  hash_table_error(std::string s) : std::runtime_error(s) {}
};

class packet_error : public std::runtime_error {
 public:
  packet_error(std::string s) : std::runtime_error(s) {}
};

#endif
