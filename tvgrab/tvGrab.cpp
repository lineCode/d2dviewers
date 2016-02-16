// tv.cpp
//{{{  includes
#include "pch.h"
#include "../common/cBda.h"
//}}}

cBda bda;
int pidCont[0xFFFF];
int pidCount[0xFFFF];

int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);

  for (int i = 0; i < 0xFFFF; i++) {
    //{{{  init
    pidCont[i] = -1;
    pidCount[i] = 0;
    }
    //}}}
  SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS);

  bda.createGraph ((argc >= 2) ? _wtoi (argv[1]) : 674000); // 650000 674000 706000
  printf ("tvGrab\n");

  HANDLE mFile = CreateFile (L"C:/Users/colin/Desktop/bda.ts",
                             GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
  int total = 0;
  int transfer = 4*240*188;

  int contOk = 0;
  for (int i = 0; i < 512; i++) {
    uint8_t* bdaBuf = bda.getSamples (transfer);

    if (false) {
      int sync = 0;
      uint8_t* tsPtr = bdaBuf;
      for (int j = 0; j < 240; j++) {
        if (*tsPtr == 0x47) {
          sync++;
          auto pid = ((*(tsPtr+1) & 0x1F) << 8) | *(tsPtr+2);
          auto continuity = *(tsPtr+3) & 0x0F;
          if ((pidCont[pid] == -1) || (pidCont[pid] == ((continuity - 1) & 0x0f)))
            contOk++;
          pidCont[pid] = continuity;
          pidCount[pid]++;
          tsPtr += 188;
          }
        }
      printf ("bda %d %4.3fm %d %d %d %llx \r",
              i, total/1000000.0, bda.getSignalStrength(), sync, contOk, (uint64_t)bdaBuf);
      }
    else
      printf ("bda %d %4.3fm %d %llx %d       \r",
              i, total/1000000.0, bda.getSignalStrength(), (uint64_t)bdaBuf, bda.hasSamples());

    DWORD written;
    WriteFile (mFile, bdaBuf, transfer, &written, NULL);
    total += transfer;
    }

  CloseHandle (mFile);

  CoUninitialize();
  }
