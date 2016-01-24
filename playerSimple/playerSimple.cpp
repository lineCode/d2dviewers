// playerSimple
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include "../common/winAudio.h"
#include "../inc/libfaad/neaacdec.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")
//}}}
#include "http.h"
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

cHlsChunk hlsChunk[3];
int playFrame = 0;
int playPhase = false;

//{{{
static DWORD WINAPI hlsLoaderThread (LPVOID arg) {

  cRadioChan* radioChan = new cRadioChan();
  radioChan->setChan (6, 128000);

  int phase = 0;
  hlsChunk[phase].load (radioChan);
  ReleaseSemaphore (hSemaphoreLoad, 1, NULL);

  while (true) {
    phase = (phase + 1) % 3;
    hlsChunk[phase].load (radioChan);
    WaitForSingleObject (hSemaphorePlay, 20 * 1000);
    }

  return 0;
  }
//}}}
//{{{
static DWORD WINAPI hlsPlayerThread (LPVOID arg) {

  WaitForSingleObject (hSemaphoreLoad, 20 * 1000);

  int phase = 0;
  while (true) {
    for (auto frame = 0; frame < hlsChunk[phase].getFrames(); frame++) {
      winAudioPlay (hlsChunk[phase].getAudioSamples (frame), hlsChunk[phase].getSamplesPerFrame()*4, 1.0f);
      playPhase = phase;
      playFrame = frame;
      appWindow->changed();
      }
    phase = (phase + 1) % 3;
    ReleaseSemaphore (hSemaphorePlay, 1, NULL);
    }

  return 0;
  }
//}}}

//{{{
bool cAppWindow::onKey (int key) {

  switch (key) {
    case 0x00 : break;
    case 0x1B : return true;

    //case 0x20 : stopped = !stopped; break;

    //case 0x21 : playFrame -= 5.0f * audFramesPerSec; changed(); break;
    //case 0x22 : playFrame += 5.0f * audFramesPerSec; changed(); break;

    //case 0x23 : playFrame = audFramesLoaded - 1.0f; changed(); break;
    //case 0x24 : playFrame = 0; break;

    //case 0x25 : playFrame -= 1.0f * audFramesPerSec; changed(); break;
    //case 0x27 : playFrame += 1.0f * audFramesPerSec; changed(); break;

    //case 0x26 : stopped = true; playFrame -=2; changed(); break;
    //case 0x28 : stopped = true; playFrame +=2; changed(); break;

    //case 0x2d : markFrame = playFrame - 2.0f; changed(); break;
    //case 0x2e : playFrame = markFrame; changed(); break;

    default   : printf ("key %x\n", key);
    }

  //if (playFrame < 0)
  //  playFrame = 0;
  //else if (playFrame >= audFramesLoaded)
  //  playFrame = audFramesLoaded - 1.0f;

  return false;
  }
//}}}
//{{{
void cAppWindow::onMouseMove (bool right, int x, int y, int xInc, int yInc) {

 // playFrame -= xInc;

 // if (playFrame < 0)
 //   playFrame = 0;
 // else if (playFrame >= audFramesLoaded)
 //   playFrame = audFramesLoaded - 1.0f;

  changed();
  }
//}}}
//{{{
void cAppWindow::onMouseUp  (bool right, bool mouseMoved, int x, int y) {

//  if (!mouseMoved)
  //  playFrame += x - (getClientF().width/2.0f);

//  if (playFrame < 0)
  //  playFrame = 0;
//  else if (playFrame >= audFramesLoaded)
 //   playFrame = audFramesLoaded - 1.0f;
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

  for (float i = 0.0f; i < getClientF().width; i++) {
    r.left = i;
    r.right = i + 1.0f;

    uint8_t org = 0;
    uint8_t len = 0;

    int frame = playFrame + (int)i - int(getClientF().width/2.0f);
    if (frame < 0) 
      hlsChunk[(playPhase+2)%3].getAudioPower (frame+300, org, len);
    else if (frame < 300)
      hlsChunk[playPhase].getAudioPower (frame, org, len);
    else if (frame < 600)
      hlsChunk[(playPhase+1)%3].getAudioPower (frame-300, org, len);

    r.top = org;
    r.bottom = (float)org + len;
    deviceContext->FillRectangle (r, getBlueBrush());
    }

  //wchar_t wStr[200];
  //swprintf (wStr, 200, L"%4.1fs - %2.0f:%d of %2d:%d",
  //          playFrame/audFramesPerSec, playFrame, vidPlayFrame, audFramesLoaded, vidFramesLoaded);
  //deviceContext->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), offset, getBlackBrush());
  //deviceContext->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), rt, getWhiteBrush());
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
