// playerSimple.cpp
//{{{  includes
#include "pch.h"
#include <thread>

#include "../common/cD2dWindow.h"

#include "../common/winAudio.h"
#include "../inc/libfaad/neaacdec.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "cHlsRadio.h"
//}}}

class cAppWindow : public cD2dWindow {
public:
  //{{{
  cAppWindow() : mPlayFrame(0), mStopped(false), mBetter(false) {
    mSemaphore = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1
    }
  //}}}
  //{{{
  ~cAppWindow() {
    CloseHandle (mSemaphore);
    }
  //}}}

  //{{{
  void run (wchar_t* title, int width, int height, int chan, int bitrate) {

    initialise (title, width, height);

    if ((bitrate != 48000) && (bitrate != 320000))
      bitrate = 128000;
    setPlayFrame (mHlsRadio.setChan (chan, bitrate) - 10.0f * cHlsChunk::getFramesPerSec());

    std::thread ([=]() { cAppWindow::loader(); } ).detach();

    auto playerThread = std::thread ([=]() { cAppWindow::player(); });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_ABOVE_NORMAL);
    playerThread.detach();

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

      case 0x21 : incPlayFrame (-60 * cHlsChunk::getFramesPerSec()); break; // page up
      case 0x22 : incPlayFrame (+60 * cHlsChunk::getFramesPerSec()); break; // page down

      //case 0x23 : break; // end
      //case 0x24 : break; // home

      case 0x25 : incPlayFrame (-2 * cHlsChunk::getFramesPerSec()); break;  // left arrow
      case 0x27 : incPlayFrame (+2 * cHlsChunk::getFramesPerSec()); break;  // right arrow

      case 0x26 : setStopped (true); incPlayFrame (-2.0f); break; // up arrow
      case 0x28 : setStopped (true); incPlayFrame (+2.0f); break; // down arrow

      //case 0x2d : break; // insert
      //case 0x2e : break; // delete

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
        int chan = (y < 272/3) ? 3 : (y < 272*2/3) ? 4 : 6;
        setPlayFrame ((float)mHlsRadio.setChan (chan, 128000) - (10.0f * cHlsChunk::getFramesPerSec()));
        signal();
        }
      }

    changed();
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* deviceContext) {

    deviceContext->Clear (D2D1::ColorF(D2D1::ColorF::Black));
    D2D1_RECT_F rt = D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height);
    D2D1_RECT_F offset = D2D1::RectF(2.0f, 2.0f, getClientF().width, getClientF().height);

    D2D1_RECT_F r = D2D1::RectF((getClientF().width/2.0f)-1.0f, 0.0f, (getClientF().width/2.0f)+1.0f, getClientF().height);
    deviceContext->FillRectangle (r, getGreyBrush());

    bool loading = false;
    int frame = getIntPlayFrame() - int(getClientF().width/2.0f);
    uint8_t* powerPtr = nullptr;
    int frames = 0;
    for (r.left = 0.0f; r.left < getClientF().width; r.left++) {
      r.right = r.left + 1.0f;
      if (!frames)
        frames = mHlsRadio.power (frame, &powerPtr, loading);
      if (frames) {
        r.top = (float)*powerPtr++;
        r.bottom = r.top + *powerPtr++;
        deviceContext->FillRectangle (r, loading ? getGreyBrush() : getBlueBrush());
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
              hours, mins, secs, mHlsRadio.getChanName(), mHlsRadio.getBitrate()/1000);
    deviceContext->DrawText (wDebugStr, (UINT32)wcslen(wDebugStr), getTextFormat(), rt, getWhiteBrush());
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
  void toggleStopped() {
    mStopped = !mStopped;
    }
  //}}}
  //{{{
  void setStopped (bool stopped) {
    mStopped = stopped;
    }
  //}}}
  //{{{
  void incPlayFrame (float inc) {
    setPlayFrame (mPlayFrame + inc);
    }
  //}}}
  //{{{
  void setPlayFrame (float frame) {
    mPlayFrame = frame;
    changed();
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
      if (mHlsRadio.load (getIntPlayFrame(), mBetter))
        Sleep (1000);
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
      winAudioPlay (mHlsRadio.play (getIntPlayFrame(), playing, seqNum), cHlsChunk::getSamplesPerFrame()*4, 1.0f);

      if (playing)
        setPlayFrame (getPlayFrame() + 1.0f);

      if (!seqNum || (seqNum != lastSeqNum)) {
        mBetter = (seqNum == (lastSeqNum + 1));
        signal();
        lastSeqNum = seqNum;
        }
      }

    winAudioClose();
    CoUninitialize();
    }
  //}}}

  //{{{  private vars
  cHlsRadio mHlsRadio;
  HANDLE mSemaphore;

  float mPlayFrame;
  bool mStopped;
  bool mBetter;
  //}}}
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

  cAppWindow appWindow;
  appWindow.run (L"hls player", 480, 272,
                 (argc >= 2) ? _wtoi(argv[1]) : 4,       // chan
                 (argc >= 3) ? _wtoi(argv[2]) : 128000); // bitrate
  }
//}}}
