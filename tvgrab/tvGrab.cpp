// tv.cpp
#include "pch.h"
#include "../common/cBda.h"

cBda bda;

int wmain (int argc, wchar_t* argv[]) {

  CoInitialize (NULL);

  bda.createGraph (674000); // 650000 674000 706000
  printf ("tvGrab\n");

  int transfer = 20*240*188;
  int total = 0;

  printf ("signal %d\n", bda.getSignalStrength());

  FILE* file = fopen ("C:\\Users\\colin\\Desktop\\bda.ts", "wb");

  for (int i = 0; i < 100; i++) {
    uint8_t* bdaBuf =  bda.getSamples (transfer);
    fwrite (bdaBuf, 1, transfer, file);
    total += transfer;
    printf ("got bda %d %4.3fm\r", i, total/1000000.0);
    }

  fclose (file);

  CoUninitialize();
  }
