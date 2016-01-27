// radio.cpp
//{{{  includes
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <thread>

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "libfaad/neaacdec.h"
#include "../common/winaudio.h"

// redefine bigHeap handlers
#define pvPortMalloc malloc
#define vPortFree free
#include "cHlsChunk.h"
//}}}
//{{{  typedef
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef signed char int8_t;
//}}}

HANDLE hSemaphore;

cHlsChunk hlsChunk[2];

//{{{
void play() {

  bool phase = false;
  while (true) {
    for (auto frame = 0; frame < hlsChunk[phase].getFramesPerChunk(); frame++)
      winAudioPlay (hlsChunk[phase].getAudioSamples (frame), hlsChunk[phase].getSamplesPerFrame()*4, 1.0f);
    phase = !phase;
    ReleaseSemaphore (hSemaphore, 1, NULL);
    }
  }
//}}}

int main (int argc, char* argv[]) {
  //{{{  init win32 stuff
  CoInitialize (NULL);
  winAudioOpen (48000, 16, 2);
  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    printf ("WSAStartup failed\n");
    exit (0);
    }
    //}}}

  hSemaphore = CreateSemaphore (NULL, 0, 1, "playSem");  // initial 0, max 1
  //}}}

  int chan = (argc >= 2) ? atoi(argv[1]) : 6;
  int bitrate = (argc >= 3) ? atoi(argv[2]) : 128000;
  printf ("radio %d %d\n", chan, bitrate);

  cRadioChan* radioChan = new cRadioChan();
  radioChan->setChan (chan);
  radioChan->getFramesPerChunk();

  printf ("radio %d %s\n", radioChan->getBaseSeqNum(), radioChan->getDateTime());

  int seqNum = radioChan->getBaseSeqNum()-1;
  bool phase = false;
  hlsChunk[phase].load (radioChan, seqNum++, bitrate);
  std::thread ([=]() { play(); } ).detach();

  while (true) {
    phase = !phase;
    hlsChunk[phase].load (radioChan, seqNum++, bitrate);
    WaitForSingleObject (hSemaphore, 20 * 1000);
    }

  //{{{  cleanup win32 stuff
  CloseHandle(hSemaphore);

  winAudioClose();
  WSACleanup();
  CoUninitialize();
  return 0;
  //}}}
  }
