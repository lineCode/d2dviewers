// tv.cpp
//{{{  includes
#include "pch.h"
#include "../common/cBda.h"
//}}}

cBda bda;
uint8_t* bdaBase = nullptr;

int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);

  SetPriorityClass (GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

  int freq = (argc >= 2) ? _wtoi (argv[1]) : 674000; // 650000 674000 706000
  bda.createGraph (freq); // 650000 674000 706000
  printf ("tvGrab %d\n", freq);

  wchar_t fileName[100];
  swprintf (fileName, 100, L"C:/Users/colin/Desktop/%d.ts", freq);
  HANDLE file = CreateFile (fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

  int total = 0;
  int transfer = 4*240*188;
  while (true) {
    uint8_t* bdaBuf = bda.getSamples (transfer);
    if (!bdaBase)
      bdaBase = bdaBuf;
    //printf ("bda %4.3fm %d %lld\r", total/1000000.0, bda.getSignalStrength(), (uint64_t)(bdaBuf- bdaBase));

    DWORD numberOfBytesWritten;
    WriteFile (file, bdaBuf, transfer, &numberOfBytesWritten, NULL);
    if (numberOfBytesWritten != transfer)
      printf ("writefile error%d %d\n", transfer, numberOfBytesWritten);

    total += transfer;
    }

  CloseHandle (file);

  CoUninitialize();
  }
