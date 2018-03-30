#include "utils/timer/timer.hpp"

namespace utils {

  Timer::Timer(const std::string &_task, std::ostream &_os, bool report)
    : start(now()), task(_task), os(_os), measureAndReport(report)
  {
  }


  double
    Timer::elapsed() const
  {
    const clock_t end = now();
    clock_t t = (end - start);
    return t * 1.0 / (double)(CLOCKS_PER_SEC);

  }

  std::ostream &Timer::printTime(std::ostream &out, clock_t clk) {
    return out << (clk * 1.0 / (double)(CLOCKS_PER_SEC));
  }


  Timer::~Timer()
  {
    if (measureAndReport)
    {
      const double difference = elapsed();
      os << task << ": " << difference << " secs\n";
    }
  }


  clock_t
    Timer::now() const
  {
    clock_t result = clock();

    return result;
  }

  // Returns elapse time in second
  double getRuntime(long* end, long* start)
  {
#if defined(_WIN32)
    return (*end - *start) / (double)(CLOCKS_PER_SEC);
#else
    return (*end - *start) / 1000000.0;
#endif
  }

  long myclock()
  {
#if defined(_WIN32)
    return clock();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000) + tv.tv_usec;
#endif
  }

} //namespace utils 
