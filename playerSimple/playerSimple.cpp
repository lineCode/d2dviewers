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

cRadioChan* radioChan = new cRadioChan();

int playFrame = 0;
int playPhase = 0;
bool stopped = false;
int16_t silence[4096];

#define CHUNKS 3
cHlsChunk hlsChunk[CHUNKS];

//{{{
static bool findFrame (int frame, int& seqNum, int& chunk, int& frameInChunk) {

  for (auto i = 0; i < CHUNKS; i++) {
    if (hlsChunk[i].contains (frame, seqNum, frameInChunk)) {
      chunk = i;
      return true;
      }
    }
  return false;
  }
//}}}
//{{{
static bool findSeqNumChunk (int seqNum, int offset, int& chunk) {

  // look for matching chunk
  chunk = 0;
  while (chunk < CHUNKS) {
    if (seqNum + offset == hlsChunk[chunk].getSeqNum())
      return true;
    chunk++;
    }

  // look for stale chunk
  chunk = 0;
  while (chunk < CHUNKS) {
    if ((hlsChunk[chunk].getSeqNum() < seqNum-(CHUNKS/2)) || (hlsChunk[chunk].getSeqNum() > seqNum+(CHUNKS/2)))
      return false;
    chunk++;
    }

  printf ("findSeqNumChunk cockup %d", seqNum);
  for (auto i = 0; i < CHUNKS; i++)
    printf (" %d", hlsChunk[i].getSeqNum());
  printf ("\n");

  chunk = 0;
  return false;
  }
//}}}
//{{{
static void invalidateChunks() {

  for (auto i = 0; i < CHUNKS; i++)
    hlsChunk[i].invalidate();
  }
//}}}

//{{{
static DWORD WINAPI hlsLoaderThread (LPVOID arg) {

  hlsChunk[0].load (radioChan, radioChan->getBaseSeqNum());
  playFrame = hlsChunk[0].getFrame();
  ReleaseSemaphore (hSemaphoreLoad, 1, NULL);

  while (true) {
    int seqNum = cHlsChunk::getFrameSeqNum (playFrame);

    bool ok = false;
    int chunk;
    if (!findSeqNumChunk (seqNum, 0, chunk))
      ok &= hlsChunk[chunk].load (radioChan, seqNum);

    for (auto i = 1; i <= CHUNKS/2; i++) {
      if (!findSeqNumChunk (seqNum, i, chunk))
        ok &= hlsChunk[chunk].load (radioChan, seqNum+i);
      if (!findSeqNumChunk (seqNum, -i, chunk))
        ok &= hlsChunk[chunk].load (radioChan, seqNum-i);
      }

    if (!ok)
      Sleep (1000);

    WaitForSingleObject (hSemaphorePlay, 20 * 1000);
    }

  return 0;
  }
//}}}
//{{{
static DWORD WINAPI hlsPlayerThread (LPVOID arg) {

  // wait for first load
  WaitForSingleObject (hSemaphoreLoad, 20 * 1000);

  int lastSeqNum = 0;
  while (true) {
    int seqNum = 0;
    int chunk = 0;
    int frameInChunk = 0;
    bool play = !stopped && findFrame (playFrame, seqNum, chunk, frameInChunk);
    winAudioPlay (play ? hlsChunk[chunk].getAudioSamples (frameInChunk) : silence, cHlsChunk::getSamplesPerFrame()*4, 1.0f);

    if (play) {
      playFrame++;
      appWindow->changed();
      }

    if (!seqNum || (seqNum != lastSeqNum)) {
      ReleaseSemaphore (hSemaphorePlay, 1, NULL);
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
      if (y < 272/3) {
        radioChan->setChan (3, 128000);
        invalidateChunks();
        ReleaseSemaphore (hSemaphorePlay, 1, NULL);
        }
      else if (y < 272*2/3) {
        radioChan->setChan (4, 128000);
        invalidateChunks();
        ReleaseSemaphore (hSemaphorePlay, 1, NULL);
        }
      else {
        radioChan->setChan (6, 128000);
        invalidateChunks();
        ReleaseSemaphore (hSemaphorePlay, 1, NULL);
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

  int frame = playFrame - int(getClientF().width/2.0f);
  for (r.left = 0.0f; r.left < getClientF().width; r.left++) {
    r.right = r.left + 1.0f;

    int seqNum = 0;
    int chunk = 0;
    int frameInChunk = 0;
    if (findFrame (frame, seqNum, chunk, frameInChunk)) {
      uint8_t org = 0;
      uint8_t len = 0;
      hlsChunk[chunk].getAudioPower (frameInChunk, org, len);
      r.top = (float)org;
      r.bottom = (float)org + len;
      deviceContext->FillRectangle (r, getBlueBrush());
      }
    frame++;
    }

  int seqNum = 0;
  int chunk = 0;
  int frameInChunk = 0;
  bool found = findFrame (playFrame, seqNum, chunk, frameInChunk);

  if (!found)
    seqNum = cHlsChunk::getFrameSeqNum (playFrame);

  wchar_t wStr[200];
  swprintf (wStr, 200, L"%d %d - %d %d %d - %d:%d:%d",
            playFrame - (radioChan->getBaseSeqNum() * 300),
            seqNum - radioChan->getBaseSeqNum(), found, chunk, frameInChunk,
            hlsChunk[0].getSeqNum() - radioChan->getBaseSeqNum(),
            hlsChunk[1].getSeqNum() - radioChan->getBaseSeqNum(),
            hlsChunk[2].getSeqNum() - radioChan->getBaseSeqNum());

  deviceContext->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), rt,
                           cHlsChunk::getLoading() ? getYellowBrush() : getWhiteBrush());
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

  radioChan->setChan (3, 128000);

  hSemaphorePlay = CreateSemaphore (NULL, 0, 1, L"playSem");  // initial 0, max 1
  hSemaphoreLoad = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1

  appWindow = cAppWindow::create (L"hls player", 480, 272);

  CreateThread (NULL, 0, hlsLoaderThread, NULL, 0, NULL);
  HANDLE hHlsPlayerThread  = CreateThread (NULL, 0, hlsPlayerThread, NULL, 0, NULL);
  SetThreadPriority (hHlsPlayerThread, THREAD_PRIORITY_ABOVE_NORMAL);

  appWindow->messagePump();

  cHlsChunk::closeDecoder();
  CoUninitialize();
  }
//}}}
