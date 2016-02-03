#include "global.h"

static LARGE_INTEGER freq;

void gettime(TIME_T* time)
{
  QueryPerformanceCounter(time);
}

int64 timediff(TIME_T* start, TIME_T* end)
{
  return (int64)((end->QuadPart - start->QuadPart));
}

void init_time(void)
{
  QueryPerformanceFrequency(&freq);
}

int64 timenorm(int64  cur_time)
{
  return (int64)(cur_time * 1000 /(freq.QuadPart));
}
