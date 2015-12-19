// timer.cpp
#include "pch.h"
#include "timer.h"

// vars
double performanceFrequency = 0.0;
__int64 performanceCounterStart = 0;


void startTimer() {

  LARGE_INTEGER largeInteger;
  QueryPerformanceFrequency (&largeInteger);
  performanceFrequency = double (largeInteger.QuadPart);

  QueryPerformanceCounter (&largeInteger);
  performanceCounterStart = largeInteger.QuadPart;
  }


double getTimer() {

  LARGE_INTEGER largeInteger;
  QueryPerformanceCounter (&largeInteger);
  return double (largeInteger.QuadPart - performanceCounterStart) / performanceFrequency;
  }
