// pch.cpp
//{{{  includes
#include "pch.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "../common/winaudio.h"
#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

// redefine bigHeap handlers
#define pvPortMalloc malloc
#define vPortFree free
#include "../common/cHlsChunk.h"
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
void player (int framesPerChunk) {

  bool phase = false;
  while (true) {
    for (auto frame = 0; frame < framesPerChunk; frame++)
      winAudioPlay (hlsChunk[phase].getAudioSamples (frame), 1024*4, 1.0f);
    phase = !phase;
    ReleaseSemaphore (hSemaphore, 1, NULL);
    }
  }
//}}}

int main (int argc, char* argv[]) {
 // init win32 stuff
  CoInitialize (NULL);
  winAudioOpen (48000, 16, 2);
  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    printf ("WSAStartup failed\n");
    exit (0);
    }
    //}}}

  int chan = (argc >= 2) ? atoi(argv[1]) : 6;
  int bitrate = (argc >= 3) ? atoi(argv[2]) : 128000;
  printf ("radio %d %d\n", chan, bitrate);

  cHttp http;
  cRadioChan radioChan;
  radioChan.setChan (&http, chan);
  int framesPerChunk = radioChan.getFramesPerChunk();
  printf ("radio seqNum:%d framesPerChunk:%d %hs\n", radioChan.getBaseSeqNum(), framesPerChunk, radioChan.getDateTime().c_str());

  // preload
  int seqNum = radioChan.getBaseSeqNum()-1;
  bool phase = false;
  hlsChunk[phase].load (&http, &radioChan, seqNum++, bitrate);

  // sync and thread
  hSemaphore = CreateSemaphore (NULL, 0, 1, "playSem");  // initial 0, max 1
  std::thread ([=]() { player (framesPerChunk); } ).detach();

  // loader
  while (true) {
    phase = !phase;
    hlsChunk[phase].load (&http, &radioChan, seqNum++, bitrate);
    printf ("radio load %d\n", seqNum-1);
    WaitForSingleObject (hSemaphore, 20 * 1000);
    }

  // cleanup win32 stuff
  CloseHandle(hSemaphore);
  winAudioClose();
  WSACleanup();
  CoUninitialize();
  return 0;
  }
