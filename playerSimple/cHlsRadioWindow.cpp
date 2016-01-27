// cHlsRadioWindow.cpp
//{{{  includes
#include "pch.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "../common/cD2dWindow.h"
#include "../common/winAudio.h"
#include "libfaad/neaacdec.h"

// redefine bigHeap handlers
#define pvPortMalloc malloc
#define vPortFree free
#include "cHlsRadio.h"
//}}}

class cHlsRadioWindow : public cD2dWindow, public cHlsRadio {
public:
  //{{{
  cHlsRadioWindow() : mPlayFrame(0), mStopped(false), mJump(false) {
    mSemaphore = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1
    }
  //}}}
  //{{{
  ~cHlsRadioWindow() {
    CloseHandle (mSemaphore);
    }
  //}}}
  //{{{
  void run (wchar_t* title, int width, int height, int chan) {

    // init window
    initialise (title, width, height);

    setPlayFrame (setChanBitrate (chan) - 10*getFramesPerSec());

    // launch loaderThread
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

      case 0x20 : toggleStopped(); break;  // space

      case 0x21 : incPlayFrame (-60*getFramesPerSec()); break; // page up
      case 0x22 : incPlayFrame (+60*getFramesPerSec()); break; // page down

      //case 0x23 : break; // end
      //case 0x24 : break; // home

      case 0x25 : incPlayFrame (-2*getFramesPerSec()); break;  // left arrow
      case 0x27 : incPlayFrame (+2*getFramesPerSec()); break;  // right arrow

      case 0x26 : setStopped (true); incPlayFrame (-2.0f); break; // up arrow
      case 0x28 : setStopped (true); incPlayFrame (+2.0f); break; // down arrow
      //case 0x2d : break; // insert
      //case 0x2e : break; // delete

      case 0x33 : setPlayFrame (setChanBitrate (3) - 10*getFramesPerSec()); break;
      case 0x34 : setPlayFrame (setChanBitrate (4) - 10*getFramesPerSec()); break;
      case 0x36 : setPlayFrame (setChanBitrate (6) - 10*getFramesPerSec()); break;
      case 0x37 : setPlayFrame (setChanBitrate (7) - 10*getFramesPerSec()); break;

      default   : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    incPlayFrame ((float)-xInc);
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {

    if (!mouseMoved) {
      if (x < 100) {
        int chan = (y < 272/4) ? 3 : (y < 272/2) ? 4 : (y < 272*3/4) ? 6 : 7;
        setPlayFrame (setChanBitrate (chan) - 10*getFramesPerSec());
        signal();
        }
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

    int frame = getIntPlayFrame() - int(getClientF().width/2.0f);
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

    // debug str
    long long secs100 = getIntPlayFrame();
    secs100 *= 1024;
    secs100 /= 480;
    int frac = secs100 % 100;
    int secs = (secs100 / 100) % 60;
    int mins = (secs100 / (60*100)) % 60;
    int hours = (int)(secs100 / (60*60*100));

    wchar_t wDebugStr[200];
    swprintf (wDebugStr, 200, L"%d:%02d:%02d %hs %dk",
              hours, mins, secs, getChanName(), getBitrate()/1000);
    dc->DrawText (wDebugStr, (UINT32)wcslen(wDebugStr), getTextFormat(), rt, getWhiteBrush());

    swprintf (wDebugStr, 200, L"%hs", getDebugStr());
    rt.left = getClientF().width/2.0f;
    dc->DrawText (wDebugStr, (UINT32)wcslen(wDebugStr), getTextFormat(), rt,
                  getLoading() ? getGreyBrush() : getWhiteBrush());
    }
  //}}}

private:
  // gets
  //{{{
  bool getStopped() {
    return mStopped;
    }
  //}}}
  //{{{
  float getPlayFrame() {
    return mPlayFrame;
    }
  //}}}
  //{{{
  int getIntPlayFrame() {
    return (int)mPlayFrame;
    }
  //}}}

  // sets
  //{{{
  void setStopped (bool stopped) {
    mStopped = stopped;
    }
  //}}}
  //{{{
  void toggleStopped() {
    mStopped = !mStopped;
    }
  //}}}
  //{{{
  void setPlayFrame (float frame) {
    mPlayFrame = frame;
    changed();
    }
  //}}}
  //{{{
  void incPlayFrame (float inc) {
    setPlayFrame (mPlayFrame + inc);
    }
  //}}}
  //{{{
  void incAlignPlayFrame (float inc) {
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

    while (true) {
      if (load (getIntPlayFrame(), mJump))
        Sleep (1000);
      mJump = false;
      wait();
      }
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);
    winAudioOpen (48000, 16, 2);

    int lastSeqNum = 0;
    while (true) {
      bool playing = !getStopped();
      int seqNum;
      winAudioPlay (getAudioSamples(getIntPlayFrame(), playing, seqNum), getSamplesPerPlay(), 1.0f);

      if (playing)
        setPlayFrame ((getIntPlayFrame() & ~(getFramesPerPlay()>> 1)) + (float)getFramesPerPlay());

      if (!seqNum || (seqNum != lastSeqNum)) {
        mJump = seqNum != lastSeqNum+1;
        if (mJump)
          // jumping around, low quality
          setBitrate (getLowBitrate());
        else if (getBitrate() == getLowBitrate())
          // normal play, better quality
          setBitrate (getMidBitrate());
        else if (getBitrate() == getMidBitrate())
          // normal play, much better quality
          setBitrate (getHighBitrate());

        signal();
        lastSeqNum = seqNum;
        }
      }

    winAudioClose();
    CoUninitialize();
    }
  //}}}

  // vars
  HANDLE mSemaphore;
  float mPlayFrame;
  bool mStopped;
  bool mJump;
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
