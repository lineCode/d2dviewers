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
HANDLE hSemaphoreLoad;

int playFrame = 0;
bool stopped = false;

cRadioChan* radioChan = new cRadioChan();
cHlsChunks hlsChunks;

//{{{
static DWORD WINAPI hlsLoaderThread (LPVOID arg) {

  while (true) {
    if (!hlsChunks.load (radioChan, playFrame))
      Sleep (1000);
    WaitForSingleObject (hSemaphoreLoad, 20 * 1000);
    }

  return 0;
  }
//}}}
//{{{
static DWORD WINAPI hlsPlayerThread (LPVOID arg) {

  int lastSeqNum = 0;
  while (true) {
    bool playing = !stopped;
    int seqNum;
    winAudioPlay (hlsChunks.play (playFrame, playing, seqNum), cHlsChunk::getSamplesPerFrame()*4, 1.0f);

    if (playing) {
      playFrame++;
      appWindow->changed();
      }

    if (!seqNum || (seqNum != lastSeqNum)) {
      ReleaseSemaphore (hSemaphoreLoad, 1, NULL);
      lastSeqNum = seqNum;
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

    case 0x21 : playFrame -= 10 * 300; changed(); break;
    case 0x22 : playFrame += 10 * 300; changed(); break;

    //case 0x23 : playFrame = audFramesLoaded - 1.0f; changed(); break;
    //case 0x24 : playFrame = 0; break;

    case 0x25 : playFrame -= 300; changed(); break;
    case 0x27 : playFrame += 300; changed(); break;

    case 0x26 : stopped = true; playFrame -=1; changed(); break;
    case 0x28 : stopped = true; playFrame +=1; changed(); break;

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
      if (y < 272/3) {
        radioChan->setChan (3, 128000);
        hlsChunks.invalidateChunks();
        ReleaseSemaphore (hSemaphoreLoad, 1, NULL);
        }
      else if (y < 272*2/3) {
        radioChan->setChan (4, 128000);
        hlsChunks.invalidateChunks();
        ReleaseSemaphore (hSemaphoreLoad, 1, NULL);
        }
      else {
        radioChan->setChan (6, 128000);
        hlsChunks.invalidateChunks();
        ReleaseSemaphore (hSemaphoreLoad, 1, NULL);
        }
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

  int seqNum = 0;
  int chunk = 0;
  int frameInChunk = 0;
  uint8_t org = 0;
  uint8_t len = 0;
  int drawFrame = playFrame - int(getClientF().width/2.0f);
  for (r.left = 0.0f; r.left < getClientF().width; r.left++) {
    r.right = r.left + 1.0f;
    if (hlsChunks.power (drawFrame++, org, len)) {
      r.top = (float)org;
      r.bottom = (float)org + len;
      deviceContext->FillRectangle (r, getBlueBrush());
      }
    }

  // debug str
  int frame = playFrame - (radioChan->getBaseSeqNum() * 300);
  bool minus = frame < 0;
  if (minus)
    frame = -frame;
  int secs100 = frame * 1024 * 100 / 48000;
  int frac = secs100 % 100;
  int secs = (secs100 / 100) % 60;
  int mins = secs100 / (60*100);
  wchar_t wDebugStr[200];
  swprintf (wDebugStr, 200, L"%c%d.%d.%d", minus?'-':'+', mins, secs, frac);
  deviceContext->DrawText (wDebugStr, (UINT32)wcslen(wDebugStr), getTextFormat(), rt,
                           cHlsChunk::getLoading() ? getYellowBrush() : getWhiteBrush());
  }
//}}}

//{{{
int wmain (int argc, wchar_t* argv[]) {

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

  hSemaphoreLoad = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1

  radioChan->setChan (3, 128000);
  playFrame = radioChan->getBaseSeqNum() * 300;

  appWindow = cAppWindow::create (L"hls player", 480, 272);

  CreateThread (NULL, 0, hlsLoaderThread, NULL, 0, NULL);
  HANDLE hHlsPlayerThread  = CreateThread (NULL, 0, hlsPlayerThread, NULL, 0, NULL);
  SetThreadPriority (hHlsPlayerThread, THREAD_PRIORITY_ABOVE_NORMAL);

  appWindow->messagePump();

  cHlsChunk::closeDecoder();
  CoUninitialize();
  }
//}}}
