// tv.cpp
#include "pch.h"
#include "../common/cBda.h"

int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);

  cBda bda;
  bda.createGraph (674000, 500000000); // 650000 674000 706000
  printf ("tvGrab\n");

  int transfer = 2000*240*188;
  int total = 0;

  uint8_t* bdaBuf =  bda.getSamples (transfer);

  FILE* file = fopen ("C:\\Users\\colin\\Desktop\\bda.ts", "wb");
  fwrite (bdaBuf, 1, transfer, file);
  printf ("got bda %4.3fm\r", transfer/1000000.0);
  fclose (file);

  CoUninitialize();
  }
