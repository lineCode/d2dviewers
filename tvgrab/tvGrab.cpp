// tv.cpp
//{{{  includes
#include "pch.h"
#include "../common/cBda.h"
//}}}

cBda bda;
uint8_t* bdaBase = nullptr;

int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);

  SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS);

  bda.createGraph ((argc >= 2) ? _wtoi (argv[1]) : 674000); // 650000 674000 706000
  printf ("tvGrab\n");

  HANDLE file = CreateFile (L"C:/Users/colin/Desktop/bda.ts",
                             GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
  int total = 0;
  int transfer = 4*240*188;

  for (int i = 0; i < 512; i++) {
    uint8_t* bdaBuf = bda.getSamples (transfer);
    if (!bdaBase) bdaBase = bdaBuf;

    if (false) {
      int sync = 0;
      uint8_t* tsPtr = bdaBuf;
      for (int j = 0; j < 4*240; j++) {
        if (*tsPtr == 0x47)
          sync++;
        tsPtr += 188;
        }
      printf ("bda %d %4.3fm %d %d %llx \r", i, total/1000000.0, bda.getSignalStrength(), sync, (uint64_t)bdaBuf);
      }
    else
      printf ("bda %d %4.3fm %d %lld  \r", i, total/1000000.0, bda.getSignalStrength(), (uint64_t)(bdaBuf- bdaBase));

    DWORD written;
    WriteFile (file, bdaBuf, transfer, &written, NULL);
    if (written != transfer)
      printf ("writefiel error%d %d\n", transfer, written);
    total += transfer;
    }

  CloseHandle (file);

  CoUninitialize();
  }
