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

  int freq = (argc >= 2) ? _wtoi (argv[1]) : 674000;

  bda.createGraph (freq); // 650000 674000 706000
  printf ("tvGrab\n");
  FILE* file = fopen ("C:\\Users\\colin\\Desktop\\bda.ts", "wb");

  int transfer = 16*240*188;
  int total = 0;

  printf ("signal %d\n", bda.getSignalStrength());

  int contOk = 0;
  for (int i = 0; i < 128; i++) {
    uint8_t* bdaBuf =  bda.getSamples (transfer);

    int sync = 0;
    uint8_t* tsPtr = bdaBuf;
    for (int j = 0; j < 240; j++) {
      if (*tsPtr == 0x47) {
        sync++;
        auto continuity = *(tsPtr+3) & 0x0F;
        auto pid = ((*(tsPtr+1) & 0x1F) << 8) | *(tsPtr+2);
        if ((pidCont[pid] == -1) || (pidCont[pid] == ((continuity - 1) & 0x0f)))
          contOk++;
        pidCont[pid] = continuity;
        pidCount[pid]++;
        tsPtr += 188;
        }
      }
    printf ("bda %d %4.3fm %d %d %d         \r", i, total/1000000.0, bda.getSignalStrength(), sync, contOk);

    fwrite (bdaBuf, 1, transfer, file);
    total += transfer;
    }

  fclose (file);

  CoUninitialize();
  }
