// playerSimple
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "../common/winAudio.h"
#include "../inc/libfaad/neaacdec.h"

#include <winsock2.h>
#include <WS2tcpip.h>

#include "cParsedUrl.h"
#include "cHttp.h"
#include "cRadioChan.h"
#include "cHlsChunk.h"
#include "cHlsRadio.h"

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

cHlsRadio hlsRadio;

float playFrame = 0;
bool stopped = false;
HANDLE hSemaphoreLoad;

//{{{
static DWORD WINAPI loaderThread (LPVOID arg) {

  while (true) {
    if (!hlsRadio.load ((int)playFrame))
      Sleep (1000);
    WaitForSingleObject (hSemaphoreLoad, 20 * 1000);
    }

  return 0;
  }
//}}}
//{{{
static DWORD WINAPI playerThread (LPVOID arg) {

  CoInitialize (NULL);
  winAudioOpen (48000, 16, 2);

  int lastSeqNum = 0;
  while (true) {
    bool playing = !stopped;
    int seqNum;
    winAudioPlay (hlsRadio.play ((int)playFrame, playing, seqNum), cHlsChunk::getSamplesPerFrame()*4, 1.0f);

    if (playing) {
      playFrame++;
      appWindow->changed();
      }

    if (!seqNum || (seqNum != lastSeqNum)) {
      ReleaseSemaphore (hSemaphoreLoad, 1, NULL);
      lastSeqNum = seqNum;
      }
    }

  winAudioClose();
  CoUninitialize();
  return 0;
  }
//}}}

//{{{
bool cAppWindow::onKey (int key) {

  switch (key) {
    case 0x00 : break;
    case 0x1B : return true;

    case 0x20 : stopped = !stopped; break;

    case 0x21 : playFrame -= 60 * cHlsChunk::getFramesPerSec(); changed(); break;
    case 0x22 : playFrame += 60 * cHlsChunk::getFramesPerSec(); changed(); break;

    //case 0x23 : playFrame = audFramesLoaded - 1.0f; changed(); break;
    //case 0x24 : playFrame = 0; break;

    case 0x25 : playFrame -= 2 * cHlsChunk::getFramesPerSec(); changed(); break;
    case 0x27 : playFrame += 2 * cHlsChunk::getFramesPerSec(); changed(); break;

    case 0x26 : stopped = true; playFrame -= 1.0f; changed(); break;
    case 0x28 : stopped = true; playFrame += 1.0f; changed(); break;

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
      int chan = (y < 272/3) ? 3 : (y < 272*2/3) ? 4 : 6;
      playFrame = (float)hlsRadio.setChan (chan, 128000) - (10.0f * cHlsChunk::getFramesPerSec());
      ReleaseSemaphore (hSemaphoreLoad, 1, NULL);
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

  int frame = (int)playFrame - int(getClientF().width/2.0f);
  uint8_t* powerPtr = nullptr;
  int frames = 0;
  for (r.left = 0.0f; r.left < getClientF().width; r.left++) {
    r.right = r.left + 1.0f;
    if (!frames)
      hlsRadio.power (frame, &powerPtr, frames);
    if (frames) {
      r.top = (float)*powerPtr++;
      r.bottom = r.top + *powerPtr++;
      deviceContext->FillRectangle (r, getBlueBrush());
      frames--;
      }
    frame++;
    }

  // debug str
  long long secs100 = (int)playFrame;
  secs100 *= 1024;
  secs100 /= 480;
  int frac = secs100 % 100;
  int secs = (secs100 / 100) % 60;
  int mins = (secs100 / (60*100)) % 60;
  int hours = (int)(secs100 / (60*60*100));

  wchar_t wDebugStr[200];
  swprintf (wDebugStr, 200, L"%d:%02d:%02d.%02d", hours, mins, secs, frac);
  deviceContext->DrawText (wDebugStr, (UINT32)wcslen(wDebugStr), getTextFormat(), rt,
                           cHlsChunk::getLoading() ? getYellowBrush() : getWhiteBrush());
  }
//}}}

//{{{
int wmain (int argc, wchar_t* argv[]) {

  //{{{
  #ifndef _DEBUG
  FreeConsole();
  #endif
  //}}}

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    printf ("WSAStartup failed\n");
    exit (0);
    }
    //}}}

  hSemaphoreLoad = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1

  playFrame = (float)hlsRadio.setChan ((argc >= 2) ? _wtoi(argv[1]) : 6, (argc >= 3) ? _wtoi(argv[2]) : 128000)
              - (10.0f * cHlsChunk::getFramesPerSec());

  appWindow = cAppWindow::create (L"hls player", 480, 272);
  HANDLE hLoaderThread = CreateThread (NULL, 0, loaderThread, NULL, 0, NULL);
  HANDLE hPlayerThread = CreateThread (NULL, 0, playerThread, NULL, 0, NULL);
  SetThreadPriority (hPlayerThread, THREAD_PRIORITY_ABOVE_NORMAL);
  appWindow->messagePump();

  cHlsChunk::closeDecoder();
  }
//}}}
