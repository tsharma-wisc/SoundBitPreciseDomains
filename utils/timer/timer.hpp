#ifndef utils_timer_timer_hpp
#define utils_timer_timer_hpp

#include <string>
#if defined(_WIN32)
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ostream>
namespace utils {

  class Timer
  {
  public:
    Timer(const std::string &task, std::ostream &os, bool report = true);
    double elapsed() const;
    ~Timer();

    static std::ostream &printTime(std::ostream &out, clock_t clk);

  private:
    const clock_t start;
    const std::string task;
    std::ostream &os;
    bool measureAndReport;

    clock_t now() const;
  };

  long myclock();

  double getRuntime(long* end, long* start);

} // namespace utils
#endif // utils_timer_timer_hpp
