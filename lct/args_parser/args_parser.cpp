#include <vector>
#include <getopt.h>
#include <iostream>

#include "lcti.hpp"

namespace lct
{
class args_parser_t
{
 public:
  struct dict_entry_t {
    std::string key;
    int val;
    dict_entry_t(const char* key_, int val_) : key(key_), val(val_) {}
  };

  void add(const std::string& name, int has_arg, int* ptr,
           const std::vector<dict_entry_t>& dict = {})
  {
    args.push_back({name, has_arg, ptr, dict});
  }

  void parse_args(int argc, char* argv[])
  {
    exe_name = argv[0];
    int long_flag;
    std::vector<struct option> long_options;
    long_options.reserve(args.size() + 1);
    for (int i = 0; i < args.size(); ++i) {
      long_options.push_back(
          {args[i].name.c_str(), args[i].has_arg, &long_flag, i});
    }
    long_options.push_back({nullptr, 0, nullptr, 0});
    while (getopt_long(argc, argv, "", long_options.data(), nullptr) == 0) {
      int i = long_flag;
      if (args[i].has_arg == no_argument) {
        *args[i].ptr = 1;
        continue;
      } else {
        bool found = false;
        for (const auto& item : args[i].dict) {
          if (item.key == optarg) {
            *args[i].ptr = item.val;
            found = true;
            break;
          }
        }
        if (!found) {
          *args[i].ptr = std::stoi(optarg);
          // printf("Assign %d to %s\n", *args[i].ptr, args[i].name.c_str());
        } else {
          // printf("Assign %d (%s) to %s\n", *args[i].ptr, optarg,
          // args[i].name.c_str());
        }
      }
    }
  }

  void print(bool verbose)
  {
    if (!verbose) {
      std::cout << R"(ArgsParser: { "exe": ")" << exe_name.c_str() << "\", ";
    }
    for (auto& arg : args) {
      std::string verbose_val;
      if (!arg.dict.empty()) {
        for (auto& item : arg.dict) {
          if (item.val == *arg.ptr) {
            verbose_val = item.key;
            break;
          }
        }
      }
      if (verbose) {
        std::cout << "ArgsParser: " << arg.name << " = " << *arg.ptr;
        if (!verbose_val.empty()) {
          std::cout << "(" << verbose_val << ")";
        }
        std::cout << std::endl;
      } else {
        if (!verbose_val.empty()) {
          std::cout << "\"" << arg.name << "\": \"" << verbose_val << "\", ";
        } else {
          std::cout << "\"" << arg.name << "\": " << *arg.ptr << ", ";
        }
      }
    }
    if (!verbose) {
      std::cout << "}\n";
    }
  }

 private:
  struct arg_t {
    std::string name;
    int has_arg;
    int* ptr;
    std::vector<dict_entry_t> dict;
  };
  std::vector<arg_t> args;
  std::string exe_name;
};
}  // namespace lct

LCT_args_parser_t LCT_args_parser_alloc()
{
  auto* parser = new lct::args_parser_t;
  return parser;
}

void LCT_args_parser_free(LCT_args_parser_t parser)
{
  auto* p = static_cast<lct::args_parser_t*>(parser);
  delete p;
}

void LCT_args_parser_add(LCT_args_parser_t parser, const char* name,
                         int has_arg, int* ptr)
{
  auto* p = static_cast<lct::args_parser_t*>(parser);
  p->add(name, has_arg, ptr);
}
void LCT_args_parser_add_dict(LCT_args_parser_t parser, const char* name,
                              int has_arg, int* ptr, LCT_dict_str_int_t dict[],
                              int count)
{
  auto* p = static_cast<lct::args_parser_t*>(parser);
  std::vector<lct::args_parser_t::dict_entry_t> dictv;
  dictv.reserve(count);
  for (int i = 0; i < count; ++i) {
    dictv.emplace_back(dict[i].key, dict[i].val);
  }
  p->add(name, has_arg, ptr, dictv);
}

void LCT_args_parser_parse(LCT_args_parser_t parser, int argc, char* argv[])
{
  auto* p = static_cast<lct::args_parser_t*>(parser);
  p->parse_args(argc, argv);
}

void LCT_args_parser_print(LCT_args_parser_t parser, bool verbose)
{
  auto* p = static_cast<lct::args_parser_t*>(parser);
  p->print(verbose);
}