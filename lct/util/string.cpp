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

uint64_t LCT_parse_arg(LCT_dict_str_int_t dict[], int count, const char* key,
                       const char* delimiter)
{
  uint64_t ret = 0;
  size_t start_pos = 0;
  bool to_break = false;
  std::string s(key);
  while (!to_break) {
    // get the word separated by delimiters
    size_t end_pos = s.find(delimiter, start_pos);
    std::string word;
    if (end_pos == std::string::npos) {
      // No more delimiter can be found
      to_break = true;
      word = s.substr(start_pos);
    } else {
      word = s.substr(start_pos, end_pos - start_pos);
    }
    start_pos = end_pos + 1;
    // process the word
    int cur_val;
    bool succeed = LCT_str_int_search(dict, count, word.c_str(), 0, &cur_val);
    if (!succeed)
      LCT_Warn(LCT_log_ctx_default, "Unknown word %s in key %s\n", word.c_str(),
               key);
    else
      ret |= (uint64_t)cur_val;
  }
  return ret;
}