// playerSimple
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "../common/winAudio.h"
#include "../inc/libfaad/neaacdec.h"

#include <winsock2.h>
#include <WS2tcpip.h>

#include "http.h"

#pragma comment (lib,"ws2_32.lib")
//}}}
//{{{
class cAppWindow : public cD2dWindow {
public:
  cAppWindow() {}
  ~cAppWindow() {}
  //{{{
  static cAppWindow* create (wchar_t* title, int width, int height) {

    cAppWindow* appWindow = new cAppWindow();
    appWindow->initialise (title, width, height);
    return appWindow;
    }
  //}}}

protected:
  bool onKey (int key);
  void onMouseMove (bool right, int x, int y, int xInc, int yInc);
  void onMouseUp (bool right, bool mouseMoved, int x, int y);
  void onDraw (ID2D1DeviceContext* deviceContext);
  };
//}}}

cAppWindow* appWindow = NULL;
HANDLE hSemaphorePlay;
HANDLE hSemaphoreLoad;

#define CHUNKS 3
cHlsChunk hlsChunk[CHUNKS];
//{{{
static bool findFrame (int frame, int& chunkIndex, int& frameInChunk) {

  for (auto i = 0; i < CHUNKS; i++) {
    if (hlsChunk[i].contains (frame, frameInChunk)) {
      chunkIndex = i;
      return true;
      }
    }
  return false;
  }
//}}}

cRadioChan* radioChan = new cRadioChan();

int playFrame = 0;
int playPhase = 0;
bool stopped = false;
int16_t silence[4096];

//{{{
static DWORD WINAPI hlsLoaderThread (LPVOID arg) {

  int phase = 0;
  hlsChunk[phase].load (radioChan);
  playFrame = hlsChunk[phase].getFrame();
  ReleaseSemaphore (hSemaphoreLoad, 1, NULL);

  while (true) {
    phase = (phase + 1) % CHUNKS;
    hlsChunk[phase].load (radioChan);
    WaitForSingleObject (hSemaphorePlay, 20 * 1000);
    }

  return 0;
  }
//}}}
//{{{
static DWORD WINAPI hlsPlayerThread (LPVOID arg) {

  // wait for first load
  WaitForSingleObject (hSemaphoreLoad, 20 * 1000);

  while (true) {
    int chunk;
    int frameInChunk;
    if (stopped)
      winAudioPlay (silence, hlsChunk[playPhase].getSamplesPerFrame()*4, 1.0f);
    else if (findFrame (playFrame, chunk, frameInChunk)) {
      winAudioPlay (hlsChunk[chunk].getAudioSamples (frameInChunk % hlsChunk[chunk].getNumFrames()),
                    hlsChunk[chunk].getSamplesPerFrame()*4, 1.0f);
      appWindow->changed();

      playFrame++;
      if (playFrame % hlsChunk[chunk].getNumFrames() == 0)
        ReleaseSemaphore (hSemaphorePlay, 1, NULL);
      }
    else {
      winAudioPlay (silence, hlsChunk[playPhase].getSamplesPerFrame()*4, 1.0f);
      playFrame++;
      }
    }

  return 0;
  }
//}}}

//{{{
bool cAppWindow::onKey (int key) {

  switch (key) {
    case 0x00 : break;
    case 0x1B : return true;

    case 0x20 : stopped = !stopped; break;

    //case 0x21 : playFrame -= 5.0f * audFramesPerSec; changed(); break;
    //case 0x22 : playFrame += 5.0f * audFramesPerSec; changed(); break;

    //case 0x23 : playFrame = audFramesLoaded - 1.0f; changed(); break;
    //case 0x24 : playFrame = 0; break;

    //case 0x25 : playFrame -= 1.0f * audFramesPerSec; changed(); break;
    //case 0x27 : playFrame += 1.0f * audFramesPerSec; changed(); break;

    case 0x26 : stopped = true; playFrame -=2; changed(); break;
    case 0x28 : stopped = true; playFrame +=2; changed(); break;

    //case 0x2d : markFrame = playFrame - 2.0f; changed(); break;
    //case 0x2e : playFrame = markFrame; changed(); break;

    default   : printf ("key %x\n", key);
    }

  return false;
  }
//}}}
//{{{
void cAppWindow::onMouseMove (bool right, int x, int y, int xInc, int yInc) {

  playFrame -= xInc;

  changed();
  }
//}}}
//{{{
void cAppWindow::onMouseUp  (bool right, bool mouseMoved, int x, int y) {

  if (!mouseMoved) {
    if (x < 100) {
      if (y < 272/3)
        radioChan->setChan (3, 128000);
      else if (y < 272*2/3)
        radioChan->setChan (4, 128000);
      else
        radioChan->setChan (6, 128000);
      }
    }

  changed();
  }
//}}}
//{{{
void cAppWindow::onDraw (ID2D1DeviceContext* deviceContext) {

  deviceContext->Clear (D2D1::ColorF(D2D1::ColorF::Black));

  D2D1_RECT_F rt = D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height);
  D2D1_RECT_F offset = D2D1::RectF(2.0f, 2.0f, getClientF().width, getClientF().height);

  D2D1_RECT_F r = D2D1::RectF((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
  deviceContext->FillRectangle (r, getGreyBrush());

  int frame = playFrame - int(getClientF().width/2.0f);
  for (r.left = 0.0f; r.left < getClientF().width; r.left++) {
    r.right = r.left + 1.0f;

    int chunk;
    int frameInChunk;
    if (findFrame (frame, chunk, frameInChunk)) {
      uint8_t org = 0;
      uint8_t len = 0;
      hlsChunk[chunk].getAudioPower (frameInChunk, org, len);
      r.top = (float)org;
      r.bottom = (float)org + len;
      deviceContext->FillRectangle (r, getBlueBrush());
      }
    frame++;
    }

  int chunk;
  int frameInChunk;
  bool found = findFrame (playFrame, chunk, frameInChunk);
  int seqNum = found ? hlsChunk[chunk].getSeqNum() : 0;

  wchar_t wStr[200];
  swprintf (wStr, 200, L"%d %d - %d %d %d", playFrame, seqNum, found, chunk, frameInChunk);
  deviceContext->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), rt, getWhiteBrush());
  }
//}}}

//{{{
int wmain (int argc, wchar_t* argv[]) {

  for (auto i = 0; i < 4096; i++)
    silence[i] = 0;

  CoInitialize (NULL);
  //{{{
  #ifndef _DEBUG
  FreeConsole();
  #endif
  //}}}

  winAudioOpen (48000, 16, 2);
  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    printf ("WSAStartup failed\n");
    exit (0);
    }
    //}}}

  radioChan->setChan (6, 128000);

  hSemaphorePlay = CreateSemaphore (NULL, 0, 1, L"playSem");  // initial 0, max 1
  hSemaphoreLoad = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1

  appWindow = cAppWindow::create (L"hls player", 480, 272);

  CreateThread (NULL, 0, hlsLoaderThread, NULL, 0, NULL);
  HANDLE hHlsPlayerThread  = CreateThread (NULL, 0, hlsPlayerThread, NULL, 0, NULL);
  SetThreadPriority (hHlsPlayerThread, THREAD_PRIORITY_ABOVE_NORMAL);

  appWindow->messagePump();

  CoUninitialize();
  }
//}}}
