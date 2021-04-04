module;

#include <chrono>

export module Utilities;

namespace Detail
{
  static thread_local uint64_t x = 123456789, y = 362436069, z = 521288629;

  uint64_t xorshf96()
  {
    x ^= x << 16;
    x ^= x >> 5;
    x ^= x << 1;

    uint64_t t = x;
    x = y;
    y = z;
    z = t ^ x ^ y;

    return z;
  }
}

export class Timer
{
public:
  Timer() : beg_(clock_::now()) {}
  void reset() { beg_ = clock_::now(); }
  double elapsed() const
  {
    return std::chrono::duration_cast<second_>
      (clock_::now() - beg_).count();
  }

private:
  typedef std::chrono::high_resolution_clock clock_;
  typedef std::chrono::duration<double, std::ratio<1> > second_;
  std::chrono::time_point<clock_> beg_;
};

export double rng()
{
  uint64_t bits = 1023ull << 52ull | Detail::xorshf96() & 0xfffffffffffffull;
  return *reinterpret_cast<double*>(&bits) - 1.0;
}

export template<typename T, typename Q>
T map(T val, Q r1s, Q r1e, Q r2s, Q r2e)
{
  return (val - r1s) / (r1e - r1s) * (r2e - r2s) + r2s;
}

export double rng(double low, double high)
{
  return map(rng(), 0.0, 1.0, low, high);
}

export void hash_combine([[maybe_unused]] std::size_t& seed) {}

export template <typename T, typename... Rest>
void hash_combine(std::size_t& seed, const T& v, Rest... rest)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  hash_combine(seed, rest...);
}