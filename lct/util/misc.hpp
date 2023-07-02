#ifndef LCI_MISC_HPP
#define LCI_MISC_HPP

#include <string>

namespace lct
{
// string
static inline std::string replaceOne(const std::string& in,
                                     const std::string& from,
                                     const std::string& to)
{
  std::string str(in);
  if (from.empty()) return str;
  size_t start_pos = str.find(from);
  if (start_pos == std::string::npos) return str;
  str.replace(start_pos, from.length(), to);
  return str;
}

static inline std::string replaceAll(const std::string in,
                                     const std::string& from,
                                     const std::string& to)
{
  std::string str(in);
  if (from.empty()) return str;
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();  // In case 'to' contains 'from', like replacing
                               // 'x' with 'yx'
  }
  return str;
}
}  // namespace lct

#endif  // LCI_MISC_HPP
