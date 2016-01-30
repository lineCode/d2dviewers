// cHlsRadioWindow.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "../common/winAudio.h"
#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

// redefine bigHeap handlers
#define pvPortMalloc malloc
#define vPortFree free
#include "../common/cHlsRadio.h"
//}}}

class cHlsRadioWindow : public cD2dWindow, public cHlsRadio {
public:
  //{{{
  cHlsRadioWindow() : mRxBytes(0) {

    mSemaphore = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1

    mSilence = (int16_t*)pvPortMalloc (getSamplesPerFrame()*2*kFramesPerPlay*2);
    memset (mSilence, 0, getSamplesPerFrame()*2*kFramesPerPlay*2);
    }
  //}}}
  //{{{
  ~cHlsRadioWindow() {

    vPortFree (mSilence);
    CloseHandle (mSemaphore);
    }
  //}}}
  //{{{
  void run (wchar_t* title, int width, int height, int chan) {

    // init window
    initialise (title, width, height);

    // launch loaderThread
    mTuneChan = chan;
    std::thread ([=]() { loader(); } ).detach();

    // launch playerThread, higher priority
    auto playerThread = std::thread ([=]() { player(); });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
    playerThread.detach();

    // loop in windows message pump till quit
    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : break;
      case 0x1B : return true; // escape

      case 0x20 : mPlaying = !mPlaying; break;  // space

      case 0x21 : incPlayFrame (getFramesFromSec(-60)); break; // page up
      case 0x22 : incPlayFrame (getFramesFromSec(+60)); break; // page down

      //case 0x23 : break; // end
      //case 0x24 : break; // home

      case 0x25 : incPlayFrame (getFramesFromSec(-2)); break;  // left arrow
      case 0x27 : incPlayFrame (getFramesFromSec(+2)); break;  // right arrow

      case 0x26 : mPlaying = false; incPlayFrame (-2); break; // up arrow
      case 0x28 : mPlaying = false; incPlayFrame (+2); break; // down arrow
      //case 0x2d : break; // insert
      //case 0x2e : break; // delete

      case 0x31 :
      case 0x32 :
      case 0x33 :
      case 0x34 :
      case 0x35 :
      case 0x36 :
      case 0x37 :
      case 0x38 :
      case 0x39 : mTuneChan = key - '0'; signal(); break;

      default   : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    incPlayFrame (-xInc);
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {

    if (!mouseMoved) {
      }

    changed();
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    dc->Clear (D2D1::ColorF(D2D1::ColorF::Black));
    D2D1_RECT_F rt = D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height);

    D2D1_RECT_F r = D2D1::RectF((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
    dc->FillRectangle (r, getGreyBrush());

    int frame = mPlayFrame - int(getClientF().width/2.0f);
    uint8_t* power = nullptr;
    int frames = 0;
    for (r.left = 0.0f; r.left < getClientF().width; r.left++) {
      r.right = r.left + 1.0f;
      if (!frames)
        power = getPower (frame, frames);
      if (power) {
        r.top = (float)*power++;
        r.bottom = r.top + *power++;
        dc->FillRectangle (r, getBlueBrush());
        frames--;
        }
      frame++;
      }

    wchar_t wDebugStr[200];
    swprintf (wDebugStr, 200, L"%hs %4.3f%c",
              getInfoStr (mPlayFrame), (mRxBytes < 1000000) ? (mRxBytes/1000.0f) : (mRxBytes/1000000.0f), mRxBytes < 1000000 ? 'k':'m');
    dc->DrawText (wDebugStr, (UINT32)wcslen(wDebugStr), getTextFormat(), rt, getWhiteBrush());
    }
  //}}}

private:
  const int kFramesPerPlay = 1;

  // sets
  //{{{
  void setPlayFrame (int frame) {
    mPlayFrame = frame;
    changed();
    }
  //}}}
  //{{{
  void incPlayFrame (int inc) {
    setPlayFrame (mPlayFrame + inc);
    }
  //}}}
  //{{{
  void incAlignPlayFrame (int inc) {
    setPlayFrame (mPlayFrame + inc);
    }
  //}}}

  // semaphore
  //{{{
  void wait() {
    WaitForSingleObject (mSemaphore, 20 * 1000);
    }
  //}}}
  //{{{
  void signal() {
    ReleaseSemaphore (mSemaphore, 1, NULL);
    }
  //}}}

  // threads
  //{{{
  void loader() {
  // loader task, handles all http gets, sleep 1s if no load suceeded

    cHttp http;
    while (true) {
      if (getChan() != mTuneChan)
        setPlayFrame (changeChan (&http, mTuneChan) - getFramesFromSec(6));
      if (!load (&http, mPlayFrame)) {
        printf ("sleep frame:%d\n", mPlayFrame);
        Sleep (1000);
        }
      mRxBytes = http.getRxBytes();
      wait();
      }
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);
    winAudioOpen (getSampleRate(), 16, 2);

    int lastSeqNum = 0;
    while (true) {
      int seqNum;
      int16_t* audioSamples = getAudioSamples (mPlayFrame, seqNum);
      winAudioPlay ((mPlaying && audioSamples) ? audioSamples : mSilence, getSamplesPerFrame()*2*kFramesPerPlay*2, 1.0f);

      if (mPlaying)
        setPlayFrame ((mPlayFrame & ~(kFramesPerPlay >> 1)) + kFramesPerPlay);

      if (!seqNum || (seqNum != lastSeqNum)) {
        setBitrateStrategy (seqNum != lastSeqNum+1);
        lastSeqNum = seqNum;
        signal();
        }
      }

    winAudioClose();
    CoUninitialize();
    }
  //}}}

  // vars
  HANDLE mSemaphore;
  int16_t* mSilence;
  int mRxBytes;
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    printf ("WSAStartup failed\n");
    exit (0);
    }
    //}}}

#ifndef _DEBUG
  FreeConsole();
#endif

  cHlsRadioWindow hlsRadioWindow;
  hlsRadioWindow.run (L"hls player", 480, 272, argc >= 2 ? _wtoi (argv[1]) : 4);
  }
//}}}
