#include "lcti.hpp"

const char* LCT_str_replace_one(const char* in, const char* from,
                                const char* to)
{
  static auto str = lct::replaceOne(in, from, to);
  return str.c_str();
}

const char* LCT_str_replace_all(const char* in, const char* from,
                                const char* to)
{
  static auto str = lct::replaceAll(in, from, to);
  return str.c_str();
}

int LCT_str_int_search(LCT_dict_str_int_t dict[], int count, const char* key,
                       int default_val, int* val)
{
  for (int i = 0; i < count; ++i) {
    bool match = false;
    if (key == nullptr || dict[i].key == nullptr) {
      if (key == nullptr && dict[i].key == nullptr) {
        match = true;
      }
    } else if (strcmp(key, dict[i].key) == 0) {
      match = true;
    }
    if (match) {
      *val = dict[i].val;
      return true;
    }
  }
  *val = default_val;
  return false;
}