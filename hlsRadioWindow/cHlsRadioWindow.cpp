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

#include "codec_def.h"
#include "codec_app_def.h"
#include "codec_api.h"
#pragma comment(lib,"welsdec.lib")

#include "../common/cVolume.h"
#include "../common/cHlsRadio.h"

#include "../common/yuvrgb_sse2.h"
//}}}

class cHlsRadioWindow : public cD2dWindow, public cVolume {
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

    mChangeToChannel = channel-1;
    mSilence = (int16_t*)pvPortMalloc (4096);
    memset (mSilence, 0, 4096);

    mPlayer = new cHlsRadio();

    // init window
    initialise (title, width, height);

    // launch loaderThread
    std::thread ([=]() { loader(); } ).detach();

    // launch playerThread, higher priority
    auto playerThread = std::thread ([=]() { player(); });
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
      case 0x39 : mChangeToChannel = key - '0' - 1; signal(); break;
      case 0x30 : mChangeToChannel = 10 - 1; signal(); break;

      default   : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    auto ratio = controlKeyDown ? 1.5f : shiftKeyDown ? 1.2f : 1.1f;
    if (delta > 0)
      ratio = 1.0f/ratio;

    setVolume ((int)(getVolume() * ratio));

    changed();
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

    auto showChannel = mShowChannel;
    mShowChannel = inClient && (x < 80);
    if (showChannel != mShowChannel)
      changed();

    if (x < 80) {
      auto channel = (y / 20) - 1;
      }
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {

    if (x < 80) {
      auto channel = (y / 20) - 1;
      if (channel < 0) {
        cHlsRadio* hlsRadio = dynamic_cast<cHlsRadio*>(mPlayer);
        if (hlsRadio)
          hlsRadio->incSourceVidBitrate (true);
        }
      else if (channel < mPlayer->getNumSource()) {
        mChangeToChannel = channel;
        signal();
        changed();
        }
      }
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    if (x > int(getClientF().width-20))
      setVolume (int(y * 100 / getClientF().height));
    else
      mPlayer->incPlaySecs (-xInc * mPlayer->getSecsPerAudFrame());
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {

    if (!mouseMoved) {
      }
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    makeBitmap (mVidFrame);
    if (mBitmap)
      dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
    else
      dc->Clear (ColorF(ColorF::Black));

    // grey mid line
    auto rMid = RectF ((getClientF().width/2)-1, 0, (getClientF().width/2)+1, getClientF().height);
    dc->FillRectangle (rMid, getGreyBrush());

    // yellow vol bar
    auto rVol= RectF (getClientF().width - 20,0, getClientF().width, getVolume() * getClientF().height/100);
    dc->FillRectangle (rVol, getYellowBrush());

    // waveform
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

    // topLine info str
    wchar_t wStr[200];
    swprintf (wStr, 200, L"%hs %4.3fm", mPlayer->getInfoStr (mPlayer->getPlaySecs()).c_str(), mHttpRxBytes/1000000.0f);
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), RectF(0,0, getClientF().width, 20), getWhiteBrush());

    if (mShowChannel)
      for (auto i = 0; i < mPlayer->getNumSource(); i++) {
        swprintf (wStr, 200, L"%hs", mPlayer->getSourceStr(i).c_str());
        dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                      RectF(0, (i+1)*20.0f, getClientF().width, (i+2)*20.0f), getWhiteBrush());
        }

    cHlsRadio* hlsRadio = dynamic_cast<cHlsRadio*>(mPlayer);
    if (hlsRadio) {
      //{{{  has hlsRadio interface, show hlsRadioInfo
      if (mShowChannel) {
        if (false) {
          // show chunk debug
          swprintf (wStr, 200, L"%hs", hlsRadio->getChunkInfoStr (0).c_str());
          dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                        RectF(0, getClientF().height-80, getClientF().width, getClientF().height), getWhiteBrush());
          swprintf (wStr, 200, L"%hs", hlsRadio->getChunkInfoStr (1).c_str());
          dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                        RectF(0, getClientF().height-60, getClientF().width, getClientF().height), getWhiteBrush());
          swprintf (wStr, 200, L"%hs", hlsRadio->getChunkInfoStr (2).c_str());
          dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                        RectF(0, getClientF().height-40, getClientF().width, getClientF().height), getWhiteBrush());
          }

        // botLine radioChan info str
        swprintf (wStr, 200, L"%hs", hlsRadio->getChannelInfoStr().c_str());
        dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                      RectF(0, getClientF().height-20, getClientF().width, getClientF().height), getWhiteBrush());
        }
      }
      //}}}
    }
  //}}}

private:
  //{{{
  void makeBitmap (cYuvFrame* yuvFrame) {

    static const D2D1_BITMAP_PROPERTIES props = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE, 96.0f, 96.0f };

    if (yuvFrame) {
      if (yuvFrame->mPts != mVidPts) {
        mVidPts = yuvFrame->mPts;
        if (mBitmap)  {
          auto pixelSize = mBitmap->GetPixelSize();
          if ((pixelSize.width != yuvFrame->mWidth) || (pixelSize.height != yuvFrame->mHeight)) {
            mBitmap->Release();
            mBitmap = nullptr;
            }
          }
        if (!mBitmap) // create bitmap
          getDeviceContext()->CreateBitmap (SizeU(yuvFrame->mWidth, yuvFrame->mHeight), props, &mBitmap);

        // allocate 16 byte aligned bgraBuf
        auto bgraBufUnaligned = malloc ((yuvFrame->mWidth * 4 * 2) + 15);
        auto bgraBuf = (uint8_t*)(((size_t)(bgraBufUnaligned) + 15) & ~0xf);

        // convert yuv420 -> bitmap bgra
        auto yPtr = yuvFrame->mYbuf;
        auto uPtr = yuvFrame->mUbuf;
        auto vPtr = yuvFrame->mVbuf;
        for (auto i = 0; i < yuvFrame->mHeight; i += 2) {
          yuv420_rgba32_sse2 (yPtr, uPtr, vPtr, bgraBuf, yuvFrame->mWidth);
          yPtr += yuvFrame->mYStride;

          yuv420_rgba32_sse2 (yPtr, uPtr, vPtr, bgraBuf + (yuvFrame->mWidth * 4), yuvFrame->mWidth);
          yPtr += yuvFrame->mYStride;
          uPtr += yuvFrame->mUVStride;
          vPtr += yuvFrame->mUVStride;

          mBitmap->CopyFromMemory (&RectU(0, i, yuvFrame->mWidth, i + 2), bgraBuf, yuvFrame->mWidth * 4);
          }

        free (bgraBufUnaligned);
        }
      }
    else if (mBitmap) {
      mBitmap->Release();
      mBitmap = nullptr;
      }
    }
  //}}}

  //{{{
  void loader() {
  // loader task, handles all http gets, sleep 1s if no load suceeded

    CoInitialize (NULL);

    cHttp http;
    while (true) {
      if (mPlayer->getSource() != mChangeToChannel) {
        mPlayer->setPlaySecs (mPlayer->changeSource (&http, mChangeToChannel) - 6.0);
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
    winAudioOpen (mPlayer->getAudSampleRate(), 16, 2);

    auto lastSeqNum = 0;
    while (true) {
      int seqNum;
      int16_t* audSamples = mPlayer->getAudSamples (mPlayer->getPlaySecs(), seqNum);
      if (audSamples && (getVolume() != 80))
        for (auto i = 0; i < 4096; i++)
          audSamples[i] = (audSamples[i] * getVolume()) / 80;

      winAudioPlay ((mPlayer->getPlaying() && audSamples) ? audSamples : mSilence, 4096, 1);
      mVidFrame = mPlayer->getVidFrame (mPlayer->getPlaySecs(), seqNum);

      if (audSamples && mPlayer->getPlaying() && !getMouseDown()) {
        mPlayer->incPlaySecs (mPlayer->getSecsPerAudFrame());
        changed();
        }

      if (!seqNum || (seqNum != lastSeqNum)) {
        (dynamic_cast<cHlsRadio*>(mPlayer))->setBitrateStrategy (seqNum != lastSeqNum+1);
        lastSeqNum = seqNum;
        signal();
        }
      }

    winAudioClose();
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

  int mChangeToChannel = 0;
  int mHttpRxBytes = 0;

  bool mShowChannel = false;
  HANDLE mSemaphore;
  int16_t* mSilence = nullptr;

  int64_t mVidPts = 0;
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
