#pragma once

#include <cmath>
#include <iterator>

template <class V>
struct Stats {
  V mean;
  V stddev;

  Stats& operator/=(V x)
  {
    mean /= x;
    stddev /= x;
    return *this;
  }

  Stats& operator*=(V x)
  {
    mean *= x;
    stddev *= x;
    return *this;
  }

  Stats& operator+=(const Stats<V>& other)
  {
    mean += other.mean;

    // Assume normal distribution!
    stddev = std::sqrt(stddev * stddev + other.stddev * other.stddev);

    return *this;
  }

  Stats operator-() const { return {-mean, stddev}; }
};

template <class V>
Stats<V> operator+(Stats<V> a, const Stats<V>& b)
{
  a += b;
  return a;
}

template <class V>
Stats<V> operator*(Stats<V> a, V x)
{
  a *= x;
  return a;
}

template <class V>
Stats<V> operator/(Stats<V> a, V x)
{
  a /= x;
  return a;
}

template <class It, class V = typename std::iterator_traits<It>::value_type>
Stats<V> stats(const It begin, const It end)
{
  V sum{};
  size_t n = 0;
  for (It i = begin; i != end; ++i) {
    sum += *i;
    ++n;
  }
  V mean = sum / n;
  V sqSum{};
  for (It i = begin; i != end; ++i) {
    V x = *i - mean;
    sqSum += x * x;
  }
  V stddev = n == 1 ? V{} : std::sqrt(sqSum / (n - 1));
  return {mean, stddev};
}
