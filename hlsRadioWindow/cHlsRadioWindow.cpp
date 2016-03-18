// cHlsRadioWindow.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

// redefine bigHeap handlers
#define pvPortMalloc malloc
#define vPortFree free

#include "codec_def.h"
#include "codec_app_def.h"
#include "codec_api.h"
#pragma comment (lib,"welsdec.lib")

#include "../common/cAudio.h"
#include "../common/cHlsRadio.h"
//}}}

class cHlsRadioWindow : public cD2dWindow, public cAudio {
public:
  //{{{
  cHlsRadioWindow() {
    mSemaphore = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1
    }
  //}}}
  //{{{
  ~cHlsRadioWindow() {
    CloseHandle (mSemaphore);
    }
  //}}}
  //{{{
  void run (wchar_t* title, int width, int height, int channel) {

    mChangeChannel = channel-1;

    mPlayer = new cHlsRadio();

    // init window
    initialise (title, width, height);

    // launch loaderThread
    thread ([=]() { loader(); } ).detach();

    // launch playerThread, higher priority
    auto playerThread = thread ([=]() { player(); });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
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

      case 0x20 : mPlayer->togglePlaying(); break;  // space

      case 0x21 : mPlayer->incPlaySecs (-5*60); changed(); break; // page up
      case 0x22 : mPlayer->incPlaySecs (+5*60); changed(); break; // page down

      //case 0x23 : break; // end
      //case 0x24 : break; // home

      case 0x25 : mPlayer->incPlaySecs (-keyInc()); changed(); break;  // left arrow
      case 0x27 : mPlayer->incPlaySecs (+keyInc()); changed(); break;  // right arrow

      case 0x26 :  // up arrow
      case 0x28 :  // down arrow
        mPlayer->setPlaying (false);
        mPlayer->incPlaySecs (((key == 0x26) ? -1.0 : 1.0) * keyInc() * mPlayer->getSecsPerVidFrame());
        changed();
        break;
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
      case 0x39 : mChangeChannel = key - '0' - 1; signal(); break;
      case 0x30 : mChangeChannel = 10 - 1; signal(); break;

      default   : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    auto ratio = mControlKeyDown ? 1.5f : mShiftKeyDown ? 1.2f : 1.1f;
    if (delta > 0)
      ratio = 1.0f / ratio;
    setVolume (getVolume() * ratio);
    changed();
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

    auto showChannel = mShowChannel;
    mShowChannel = inClient && (x < 80);
    if (showChannel != mShowChannel)
      changed();

    mProxChannel = -1;
    if (x < 80) {
      float yoff = 20.0f;
      for (auto source = 0; source < mPlayer->getNumSource(); source++) {
        if (source != mPlayer->getSource()) {
          if ((y >= yoff) && (y < yoff + 20.0f)) {
            mProxChannel = source;
            return;
            }
          yoff += 20.0f;
          }
        }
      }
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {

    mScrubbing = false;
    if (x < 80) {
      if (mProxChannel < 0) {
        cHlsRadio* hlsRadio = dynamic_cast<cHlsRadio*>(mPlayer);
        if (hlsRadio) {
          hlsRadio->incSourceVidBitrate (true);
          mDownConsumed = true;
          }
        }
      else if (mProxChannel < mPlayer->getNumSource()) {
        mDownConsumed = true;
        mChangeChannel = mProxChannel;
        signal();
        changed();
        }
      }
    else if (x > int(getClientF().width-20)) {
      mDownConsumed = true;
      setVolume (y / (getClientF().height * 0.8f));
      changed();
      }
    else
      mScrubbing = true;
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    if (x > int(getClientF().width-20)) {
      setVolume (y / (getClientF().height * 0.8f));
      changed();
      }
    else {
      mPlayer->incPlaySecs (-xInc * mPlayer->getSecsPerAudFrame());
      changed();
      }
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {
    mScrubbing = false;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (makeBitmap (mVidFrame, mBitmap, mBitmapPts))
      dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
    else
      dc->Clear (ColorF(ColorF::Black));

    //{{{  grey mid line
    auto rMid = RectF ((getClientF().width/2)-1, 0, (getClientF().width/2)+1, getClientF().height);
    dc->FillRectangle (rMid, getGreyBrush());
    //}}}
    //{{{  yellow volume bar
    auto rVol= RectF (getClientF().width - 20,0, getClientF().width, getVolume() * 0.8f * getClientF().height);
    dc->FillRectangle (rVol, getYellowBrush());
    //}}}
    //{{{  blue waveform
    auto secs = mPlayer->getPlaySecs() - (mPlayer->getSecsPerAudFrame() * getClientF().width / 2.0);
    uint8_t* power = nullptr;
    auto frames = 0;
    auto rWave = RectF (0,0,1,0);
    for (; rWave.left < getClientF().width; rWave.left++, rWave.right++) {
      if (!frames)
        power = mPlayer->getPower (secs, frames);
      if (power) {
        rWave.top = (float)*power++;
        rWave.bottom = rWave.top + *power++;
        dc->FillRectangle (rWave, getBlueBrush());
        frames--;
        }
      secs += mPlayer->getSecsPerAudFrame();
      }
    //}}}

    //{{{  topLine info text
    wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
    wstring wstr = converter.from_bytes (mPlayer->getInfoStr (mPlayer->getPlaySecs()));
    wstr += L" " + to_wstring(mHttpRxBytes/1000) + L"k";
    dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), RectF(0,0, getClientF().width, 20), getWhiteBrush());
    //}}}
    if (mShowChannel) {
      //{{{  show sources
      float y = 20.0f;
      for (auto source = 0; source < mPlayer->getNumSource(); source++) {
        if (source != mPlayer->getSource()) {
          wstring wstr = converter.from_bytes (mPlayer->getSourceStr (source));
          dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(),
                        RectF (0, y, getClientF().width, y + 20.0f),
                        (source == mProxChannel) ? getWhiteBrush() : getGreyBrush());
          y += 20.0f;
          }
        }
      }
      //}}}

    // hlsRadio debug text
    auto hlsRadio = dynamic_cast<cHlsRadio*>(mPlayer);
    if (hlsRadio) {
      //{{{  has hlsRadio interface, show hlsRadioInfo
      if (mShowChannel) {
        if (false) {
          // show chunk debug
          wstring wstr0 = converter.from_bytes (hlsRadio->getChunkInfoStr(0));
          wstring wstr1 = converter.from_bytes (hlsRadio->getChunkInfoStr(0));
          wstring wstr2 = converter.from_bytes (hlsRadio->getChunkInfoStr(0));
          dc->DrawText (wstr0.data(), (uint32_t)wstr0.size(), getTextFormat(),
                        RectF(0, getClientF().height-80, getClientF().width, getClientF().height), getWhiteBrush());
          dc->DrawText (wstr1.data(), (uint32_t)wstr1.size(), getTextFormat(),
                        RectF(0, getClientF().height-60, getClientF().width, getClientF().height), getWhiteBrush());
          dc->DrawText (wstr2.data(), (uint32_t)wstr2.size(), getTextFormat(),
                        RectF(0, getClientF().height-40, getClientF().width, getClientF().height), getWhiteBrush());
          }

        // botLine radioChan info str
        wstring wstr = converter.from_bytes (hlsRadio->getChannelInfoStr());
        dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(),
                      RectF(0, getClientF().height-20, getClientF().width, getClientF().height), getWhiteBrush());
        }
      }
      //}}}
    }
  //}}}

private:
  //{{{
  void loader() {
  // loader task, handles all http gets, sleep 1s if no load suceeded

    CoInitialize (NULL);

    cHttp http;
    while (true) {
      if (mPlayer->getSource() != mChangeChannel) {
        mPlayer->setPlaySecs (mPlayer->changeSource (&http, mChangeChannel) - 6.0);
        changed();
        }
      if (!mPlayer->load (getDeviceContext(), &http, mPlayer->getPlaySecs())) {
        printf ("sleep frame:%3.2f\n", mPlayer->getPlaySecs());
        sleep (1000);
        }
      mHttpRxBytes = http.getRxBytes();
      wait();
      }

    CoUninitialize();
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);
    audOpen (mPlayer->getAudSampleRate(), 16, 2);

    auto lastSeqNum = 0;
    while (true) {
      int seqNum;
      auto audSamples = mPlayer->getAudSamples (mPlayer->getPlaySecs(), seqNum);
      audPlay (mPlayer->getPlaying() ? audSamples : nullptr, 4096, 1.0f);
      mVidFrame = mPlayer->getVidFrame (mPlayer->getPlaySecs(), seqNum);

      if (audSamples && mPlayer->getPlaying() && !mScrubbing) {
        mPlayer->incPlaySecs (mPlayer->getSecsPerAudFrame());
        changed();
        }

      if (!seqNum || (seqNum != lastSeqNum)) {
        (dynamic_cast<cHlsRadio*>(mPlayer))->setBitrateStrategy (seqNum != lastSeqNum+1);
        lastSeqNum = seqNum;
        signal();
        }
      }

    audClose();
    CoUninitialize();
    }
  //}}}

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
  //{{{
  void sleep (int ms) {
    Sleep (ms);
    }
  //}}}

  //{{{  vars
  iPlayer* mPlayer = nullptr;

  bool mScrubbing = false;
  int mProxChannel = 0;
  int mChangeChannel = 0;

  int mHttpRxBytes = 0;

  bool mShowChannel = false;

  HANDLE mSemaphore;

  int64_t mBitmapPts = 0;
  cYuvFrame* mVidFrame = nullptr;
  ID2D1Bitmap* mBitmap = nullptr;
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

  cHlsRadioWindow hlsRadioWindow;
  hlsRadioWindow.run (L"hlsRadioWindow", 480, 272, argc >= 2 ? _wtoi (argv[1]) : 4);
  }
//}}}
