// tv.cpp
//{{{  includes
#include "pch.h"
#include "../common/cBda.h"
//}}}

int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);
  SetPriorityClass (GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

  int freq = (argc >= 2) ? _wtoi (argv[1]) : 674000; // 650000 674000 706000

  cBda bda (128*240*188);
  bda.createGraph (freq);
  printf ("tvGrab %d\n", freq);

  wchar_t fileName[100];
  swprintf (fileName, 100, L"C:/Users/colin/Desktop/%d.ts", freq);
  HANDLE file = CreateFile (fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

  int blockLen = 0;
  while (true) {
    uint8_t* ptr = bda.getContiguousBlock (blockLen);
    if (blockLen) {
      DWORD numberOfBytesWritten;
      WriteFile (file, ptr, blockLen, &numberOfBytesWritten, NULL);
      if (numberOfBytesWritten != blockLen)
        printf ("writefile error%d %d\n", blockLen, numberOfBytesWritten);
      bda.decommitBlock (blockLen);
      }
    else
      Sleep (1);
    }

  CloseHandle (file);

  CoUninitialize();
  }
