// tv.cpp
#include "pch.h"
#include "../common/cBda.h"

int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);

  // 650000 674000 706000
  cBda bda;
  uint8_t* bdaBuf = bda.createBDAGraph (674000, 500000000);
  printf ("tvGrab bda %x\n\n", (int)bdaBuf);

  int transfer = 2000*240*188;
  int total = 0;

  bdaBuf =  bda.getBda (transfer);

  FILE* file = fopen ("C:\\Users\\colin\\Desktop\\bda.ts", "wb");
  fwrite (bdaBuf, 1, transfer, file);
  printf ("got bda %x %4.3fm\r", (int)bdaBuf, transfer/1000000.0);
  fclose (file);

  CoUninitialize();
  }
