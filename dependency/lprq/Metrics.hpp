#pragma once

#include <unordered_map>
#include <string>
#include <numeric>
#include <vector>
#include <ostream>
#include <mutex>
#include "Stats.hpp"

class Metrics
{
 private:
  std::unordered_map<std::string, size_t> metrics{};

 public:
  size_t& operator[](const std::string& metric) { return metrics[metric]; }

  size_t operator[](const std::string& metric) const
  {
    auto it = metrics.find(metric);
    if (it == metrics.end()) return 0;
    return it->second;
  }

  const std::unordered_map<std::string, size_t>& data() const
  {
    return metrics;
  }

  void reset() { metrics.clear(); }

  Metrics& operator+=(const Metrics& other)
  {
    for (auto [key, value] : other.metrics) {
      metrics[key] += value;
    }
    return *this;
  }

  friend std::ostream& operator<<(std::ostream& stream, const Metrics& metrics);
};

Metrics operator+(const Metrics& a, const Metrics& b)
{
  Metrics res;
  res += a;
  res += b;
  return res;
}

std::ostream& operator<<(std::ostream& stream, const Metrics& metrics)
{
  for (auto [key, value] : metrics.metrics) {
    stream << key << ": " << value << '\n';
  }
  return stream;
}

template <class It>
std::unordered_map<std::string, Stats<long double>> metricStats(const It begin,
                                                                const It end)
{
  std::unordered_map<std::string, std::vector<long double>> data;
  for (It it = begin; it != end; ++it) {
    for (auto [key, value] : it->data()) {
      data[key].push_back(static_cast<long double>(value));
    }
  }

  std::unordered_map<std::string, Stats<long double>> res;
  for (const auto& [key, values] : data) {
    res[key] = stats(values.begin(), values.end());
  }
  return res;
}

class MetricsCollector
{
 private:
  std::unordered_map<std::string, std::vector<size_t>> allMetrics;
  std::mutex mutex;
  size_t numThreads;

 public:
  class Accessor
  {
   private:
    size_t* tlMetrics;

   public:
    explicit Accessor(size_t* tlMetrics) : tlMetrics(tlMetrics) {}

    void inc(const size_t value, int tid) { tlMetrics[tid] += value; }
  };

  explicit MetricsCollector(size_t numThreads) : numThreads(numThreads) {}
  MetricsCollector(const Metrics&) = delete;
  MetricsCollector(Metrics&&) = delete;
  MetricsCollector& operator=(const Metrics&) = delete;
  MetricsCollector& operator=(Metrics&&) = delete;

  Accessor accessor(std::string metric)
  {
    std::lock_guard lock(mutex);
    std::vector<size_t>& tlMetrics = allMetrics[std::move(metric)];
    if (tlMetrics.size() < numThreads) tlMetrics.resize(numThreads, 0);
    return Accessor(tlMetrics.data());
  }

  Metrics combine()
  {
    Metrics res;
    std::lock_guard lock(mutex);
    for (const auto& [key, tlMetrics] : allMetrics) {
      res[key] = std::reduce(tlMetrics.begin(), tlMetrics.end());
    }
    return res;
  }

  void reset(int tid)
  {
    std::lock_guard lock(mutex);
    for (auto& [key, tlMetrics] : allMetrics) {
      tlMetrics[tid] = 0;
    }
  }

  void reset()
  {
    std::lock_guard lock(mutex);
    allMetrics.clear();
  }
};

class MetricsAwareBase
{
 private:
  MetricsCollector collector;

 protected:
  MetricsCollector::Accessor accessor(std::string metric)
  {
    return collector.accessor(std::move(metric));
  }

 public:
  explicit MetricsAwareBase(size_t numThreads) : collector(numThreads) {}

  Metrics collectMetrics() { return collector.combine(); }

  void resetMetrics(int tid) { collector.reset(tid); }

  void resetMetrics() { collector.reset(); }
};
