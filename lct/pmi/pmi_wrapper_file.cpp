#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdexcept>
#include <algorithm>
#include <sys/file.h>
#include <chrono>
#include <thread>
#include "lcti.hpp"
#include "pmi_wrapper.hpp"

// The "file" method uses shared files to mimic the PMI interface.
// It is not meant to be scalable, but it is useful for testing.

namespace lct
{
namespace pmi
{
namespace file_e
{
namespace detail
{
int get_nranks_from_env()
{
  static int nranks = 0;
  if (nranks > 0) {
    return nranks;
  }
  // try LCT_PMI_FILE_NRANKS
  const char* nranks_str = getenv("LCT_PMI_FILE_NRANKS");
  if (nranks_str) {
    nranks = atoi(nranks_str);
    return nranks;
  }
  // try SLURM_NTASKS
  const char* ntasks_str = getenv("SLURM_NTASKS");
  if (ntasks_str) {
    nranks = atoi(ntasks_str);
    return nranks;
  }
  return nranks;
}

bool isNumber(const std::string& str)
{
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

void ensure_directory_exists(const std::string& dirname)
{
  // Create the directory if it does not exist.
  struct stat st;
  if (stat(dirname.c_str(), &st) == -1) {
    // Attempt to create the directory
    if (mkdir(dirname.c_str(), 0755) == -1) {
      LCT_Assert(LCT_log_ctx_default, errno == EEXIST,
                 "Failed to create directory: %s (errno: %d)", dirname.c_str(),
                 errno);
    } else {
      LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi_file",
              "Created directory: %s\n", dirname.c_str());
    }
  } else if (!S_ISDIR(st.st_mode)) {
    LCT_Assert(LCT_log_ctx_default, false, "%s exists but is not a directory.",
               dirname.c_str());
  }
}

void ensure_path_exists(const std::string& path)
{
  // Recursively create the directory if it does not exist.
  size_t pos = 0;
  while (true) {
    pos = path.find('/', pos + 1);
    if (pos == std::string::npos) {
      break;
    }
    ensure_directory_exists(path.substr(0, pos));
  }
}

std::string get_dirname()
{
  // By default, we use ~/.tmp/lct_pmi_file-<jobid> as the directory name.
  uint64_t jobid = 0;
  const char* jobid_str = getenv("SLURM_JOBID");
  if (jobid_str) {
    jobid = strtoull(jobid_str, NULL, 10);
  }
  const char* home_path = getenv("HOME");
  LCT_Assert(LCT_log_ctx_default, home_path,
             "HOME environment variable is not set.");
  std::string dirname = std::string(home_path) + "/.tmp/lct_pmi_file-" +
                        std::to_string(jobid) + "/";
  return dirname;
}

bool try_lock_file(int fd)
{
  int ret = flock(fd, LOCK_EX | LOCK_NB);
  if (ret == 0) {
    return true;
  } else if (errno == EWOULDBLOCK) {
    return false;
  } else {
    LCT_Assert(LCT_log_ctx_default, false, "Failed to lock file (errno: %d)",
               errno);
    return false;
  }
}

void lock_file(int fd)
{
  int ret = flock(fd, LOCK_EX);
  LCT_Assert(LCT_log_ctx_default, ret == 0, "Failed to lock file (errno: %d)",
             errno);
}

void unlock_file(int fd)
{
  int ret = flock(fd, LOCK_UN);
  LCT_Assert(LCT_log_ctx_default, ret == 0, "Failed to unlock file (errno: %d)",
             errno);
}

std::string read_file(int fd, bool from_beginning = false)
{
  std::string content;
  const int read_len = 16;
  if (from_beginning) {
    lseek(fd, 0, SEEK_SET);
  }
  while (true) {
    char line[read_len];
    memset(line, 0, read_len);
    ssize_t ret = read(fd, line, read_len);
    if (ret <= 0) {
      break;
    }
    content += std::string(line);
  }
  return content;
}

void write_file(int fd, const std::string& content)
{
  ssize_t ret = write(fd, content.c_str(), content.size());
  LCT_Assert(LCT_log_ctx_default, ret == content.size(),
             "Error writing file (ret: %d; errno: %d %s): %s", ret, errno,
             strerror(errno), content.c_str());
}

void reset_file(int fd, const std::string& content)
{
  lseek(fd, 0, SEEK_SET);
  int ret = ftruncate(fd, 0);
  LCT_Assert(LCT_log_ctx_default, ret == 0,
             "Error truncating file (errno: %d %s)", errno, strerror(errno));
  write_file(fd, content);
}

}  // namespace detail

int g_rank = -1;
int g_nranks = -1;

int check_availability()
{
  // We don't need to use this if we only have one rank.
  if (detail::get_nranks_from_env() > 1)
    return true;
  else
    return false;
}

void initialize()
{
  // Get the expected number of processes from the environment.
  g_nranks = detail::get_nranks_from_env();
  LCT_Assert(LCT_log_ctx_default, g_nranks > 0,
             "Failed to get the number of ranks from the environment.");

  // get the filename
  std::string dirname = detail::get_dirname();
  detail::ensure_path_exists(dirname);

  // Get nranks
  std::string filename_nranks = dirname + "nranks";
  int fd_nranks = open(filename_nranks.c_str(), O_CREAT | O_RDWR, 0644);
  LCT_Assert(LCT_log_ctx_default, fd_nranks != -1, "Error opening file: %s",
             filename_nranks.c_str());
  detail::lock_file(fd_nranks);
  // Read the number of ranks from the file
  auto content = detail::read_file(fd_nranks, true);
  if (content.empty()) {
    g_rank = 0;
    detail::reset_file(fd_nranks, "1");
  } else {
    g_rank = std::stoi(content);
    detail::reset_file(fd_nranks, std::to_string(g_rank + 1));
  }
  detail::unlock_file(fd_nranks);
  LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi_file",
          "Assigned as rank %d/%d\n", g_rank, g_nranks);
  LCT_Assert(LCT_log_ctx_default, g_rank < g_nranks,
             "Error: Rank %d is greater than the number of ranks %d. Remove "
             "the %s and try again\n",
             g_rank, g_nranks, dirname.c_str());

  std::string filename_barrier = dirname + "barrier";
  std::string filename_data = dirname + "data";
  if (g_rank == g_nranks - 1) {
    // Create the barrier file
    int fd = open(filename_barrier.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    LCT_Assert(LCT_log_ctx_default, fd != -1, "Error creating file: %s",
               filename_barrier.c_str());
    detail::reset_file(fd, "0 0");
    close(fd);
    // Create the data file
    fd = open(filename_data.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    LCT_Assert(LCT_log_ctx_default, fd != -1, "Error creating file: %s",
               filename_data.c_str());
    close(fd);
    // reset the nranks file to 0
    detail::reset_file(fd_nranks, "0");
  } else {
    // Wait for the nranks to be reset to 0
    while (true) {
      auto content = detail::read_file(fd_nranks, true);
      if (content == "0") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
  close(fd_nranks);
  LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi_file",
          "Rank %d/%d initialized\n", g_rank, g_nranks);
}

int initialized() { return g_rank != -1; }
int get_rank() { return g_rank; }

int get_size() { return g_nranks; }

void publish(char* key, char* value)
{
  // write to the data file
  std::string dirname = detail::get_dirname();
  std::string filename_data = dirname + "data";
  int fd = open(filename_data.c_str(), O_WRONLY | O_APPEND, 0644);
  LCT_Assert(LCT_log_ctx_default, fd != -1, "Error opening file: %s",
             filename_data.c_str());
  detail::write_file(fd, std::string(key) + " " + std::string(value) + "\n");
  close(fd);
}

void getname(int rank, char* key, char* value)
{
  // read from the data file
  std::string dirname = detail::get_dirname();
  std::string filename_data = dirname + "data";
  std::fstream file(filename_data, std::ios::in);
  LCT_Assert(LCT_log_ctx_default, file.is_open(), "Error opening file: %s",
             filename_data.c_str());
  std::string line;
  bool found = false;
  while (std::getline(file, line)) {
    std::string::size_type pos = line.find(' ');
    if (pos == std::string::npos) {
      LCT_Assert(LCT_log_ctx_default, false, "Error reading line: %s",
                 line.c_str());
    }
    std::string key_str = line.substr(0, pos);
    if (key_str == std::string(key)) {
      std::string value_str = line.substr(pos + 1);
      strncpy(value, value_str.c_str(), LCT_PMI_STRING_LIMIT);
      found = true;
      break;
    }
  }
  LCT_Assert(LCT_log_ctx_default, found, "Error: Key %s not found in file: %s",
             key, filename_data.c_str());
  file.close();
}

void barrier()
{
  // read from the barrier file
  std::string dirname = detail::get_dirname();
  std::string filename_barrier = dirname + "barrier";
  int fd = open(filename_barrier.c_str(), O_RDWR, 0644);
  LCT_Assert(LCT_log_ctx_default, fd != -1, "Error opening file: %s",
             filename_barrier.c_str());

  detail::lock_file(fd);
  auto content = detail::read_file(fd, true);
  auto pos = content.find(' ');
  LCT_Assert(LCT_log_ctx_default, pos != std::string::npos,
             "Error reading file: %s", content.c_str());
  int tag = std::stoi(content.substr(0, pos));
  int count = std::stoi(content.substr(pos + 1));
  LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi_file",
          "Reach the Barrier: tag %d count %d/%d\n", tag, count, g_nranks);
  if (count == g_nranks - 1) {
    // Reset the count and increment the tag
    detail::reset_file(fd, std::to_string(tag + 1) + " 0");
    detail::unlock_file(fd);
  } else {
    // write count back to the file
    detail::reset_file(fd,
                       std::to_string(tag) + " " + std::to_string(count + 1));
    detail::unlock_file(fd);

    // wait for the tag to change
    int sleep_time = 100;
    int max_sleep_time = 1000;
    while (true) {
      detail::lock_file(fd);
      auto content = detail::read_file(fd, true);
      auto pos = content.find(' ');
      LCT_Assert(LCT_log_ctx_default, pos != std::string::npos,
                 "Error reading file: %s", content.c_str());
      int new_tag = std::stoi(content.substr(0, pos));
      detail::unlock_file(fd);
      if (new_tag != tag) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
      sleep_time = std::min(sleep_time * 2, max_sleep_time);
    }
  }
  LCT_Log(LCT_log_ctx_default, LCT_LOG_INFO, "pmi_file",
          "Barrier Done: tag %d\n", tag);
}

void finalize()
{
  g_rank = -1;
  g_nranks = -1;
  return;
}

}  // namespace file_e

void file_setup_ops(struct ops_t* ops)
{
  ops->check_availability = file_e::check_availability;
  ops->initialize = file_e::initialize;
  ops->is_initialized = file_e::initialized;
  ops->get_rank = file_e::get_rank;
  ops->get_size = file_e::get_size;
  ops->publish = file_e::publish;
  ops->getname = file_e::getname;
  ops->barrier = file_e::barrier;
  ops->finalize = file_e::finalize;
}
}  // namespace pmi
}  // namespace lct