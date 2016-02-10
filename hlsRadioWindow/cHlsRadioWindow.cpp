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

#include "yuvrgb_sse2.h"
//}}}

class cHlsRadioWindow : public cD2dWindow, public cVolume {
public:
  //{{{
  cHlsRadioWindow() : mPlayer(nullptr), mChangeToChannel(0),
                      mHttpRxBytes(0), mShowChan(false), mVidFrame(nullptr), mBitmap(nullptr), mVidId(-1) {

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
  int keyInc() {
    return controlKeyDown ? 60*60 : shiftKeyDown ? 60 : 1;
    }
  //}}}
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : break;
      case 0x1B : return true; // escape

      case 0x20 : mPlayer->togglePlaying(); break;  // space

      case 0x21 : mPlayer->incPlayFrame (mPlayer->getAudFramesFromSec (-5*60)); changed(); break; // page up
      case 0x22 : mPlayer->incPlayFrame (mPlayer->getAudFramesFromSec (+5*60)); changed(); break; // page down

      //case 0x23 : break; // end
      //case 0x24 : break; // home

      case 0x25 : mPlayer->incPlayFrame (mPlayer->getAudFramesFromSec (-keyInc())); changed(); break;  // left arrow
      case 0x27 : mPlayer->incPlayFrame (mPlayer->getAudFramesFromSec (+keyInc())); changed(); break;  // right arrow

      case 0x26 : mPlayer->setPlaying (false); mPlayer->incPlayFrame (-keyInc()); changed(); break; // up arrow
      case 0x28 : mPlayer->setPlaying (false); mPlayer->incPlayFrame (+keyInc()); changed(); break; // down arrow
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

    float ratio = controlKeyDown ? 1.5f : shiftKeyDown ? 1.2f : 1.1f;
    if (delta > 0)
      ratio = 1.0f/ratio;

    setVolume ((int)(getVolume() * ratio));

    changed();
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

    bool showChan = mShowChan;
    mShowChan = inClient && (x < 80);
    if (showChan != mShowChan)
      changed();

    if (x < 80) {
      int channel = (y / 20) - 1;
      }
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {
    if (x < 80) {
      int channel = (y / 20) - 1;
      if ((chan >= 0) && (chan < mPlayer->getNumSource())) {
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
      mPlayer->incPlayFrame (-xInc);
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

    if (makeBitmap())
      dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
    else
      dc->Clear (ColorF(ColorF::Black));

    // grey mid line
    D2D1_RECT_F rMid = RectF ((getClientF().width/2)-1, 0, (getClientF().width/2)+1, getClientF().height);
    dc->FillRectangle (rMid, getGreyBrush());

    // yellow vol bar
    D2D1_RECT_F rVol= RectF (getClientF().width - 20,0, getClientF().width, getVolume() * getClientF().height/100);
    dc->FillRectangle (rVol, getYellowBrush());

    // waveform
    int frame = mPlayer->getPlayFrame() - int(getClientF().width/2);
    uint8_t* power = nullptr;
    int frames = 0;
    D2D1_RECT_F rWave = RectF (0,0,1,0);
    for (; rWave.left < getClientF().width; rWave.left++, rWave.right++, frame++) {
      if (!frames)
        power = mPlayer->getPower (frame, frames);
      if (power) {
        rWave.top = (float)*power++;
        rWave.bottom = rWave.top + *power++;
        dc->FillRectangle (rWave, getBlueBrush());
        frames--;
        }
      }

    // topLine info str
    wchar_t wStr[200];
    swprintf (wStr, 200, L"%hs %4.3fm", mPlayer->getInfoStr (mPlayer->getPlayFrame()).c_str(), mHttpRxBytes/1000000.0f);
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(), RectF(0,0, getClientF().width, 20), getWhiteBrush());

    if (mShowChan)
      for (auto i = 0; i < mPlayer->getNumSource(); i++) {
        swprintf (wStr, 200, L"%hs", mPlayer->getSourceStr(i).c_str());
        dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                      RectF(0, (i+1)*20.0f, getClientF().width, (i+2)*20.0f), getWhiteBrush());
        }

    cHlsRadio* hlsRadio = dynamic_cast<cHlsRadio*>(mPlayer);
    if (hlsRadio) {
      //{{{  has hlsRadio interface, show hlsRadioInfo
      if (mShowChan) {
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
  bool makeBitmap() {

    if (mVidFrame) {
      if (mVidFrame->mId != mVidId) {
        mVidId = mVidFrame->mId;
        if (!mBitmap) {
          // create bitmap, should check size
          D2D1_BITMAP_PROPERTIES d2d1_bitmapProperties;
          d2d1_bitmapProperties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
          d2d1_bitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
          d2d1_bitmapProperties.dpiX = 96.0f;
          d2d1_bitmapProperties.dpiY = 96.0f;
          getDeviceContext()->CreateBitmap (SizeU (mVidFrame->mWidth, mVidFrame->mHeight), d2d1_bitmapProperties, &mBitmap);
          }

        // vidFrame yuv420 -> bitmap bgra
        uint8_t* bgraBuf = (uint8_t*)malloc (2*mVidFrame->mWidth * 4);

        uint8_t* yPtr = mVidFrame->mYbuf;
        uint8_t* uPtr = mVidFrame->mUbuf;
        uint8_t* vPtr = mVidFrame->mVbuf;
        D2D1_RECT_U r(RectU (0, 0, mVidFrame->mWidth, 2));
        for (int i = 0; i < mVidFrame->mHeight/2; i++) {
          yuv420_rgba32_sse2 (yPtr, uPtr, vPtr, bgraBuf, mVidFrame->mWidth);
          yPtr += mVidFrame->mYStride;
          yuv420_rgba32_sse2 (yPtr, uPtr, vPtr, bgraBuf + (mVidFrame->mWidth * 4), mVidFrame->mWidth);
          yPtr += mVidFrame->mYStride;
          uPtr += mVidFrame->mUVStride;
          vPtr += mVidFrame->mUVStride;
          mBitmap->CopyFromMemory (&r, bgraBuf, mVidFrame->mWidth * 4);
          r.top += 2;
          r.bottom = r.top + 2;
          }

        free (bgraBuf);
        }
      return true;
      }
    else
      return false;
    }
  //}}}

  //{{{
  void loader() {
  // loader task, handles all http gets, sleep 1s if no load suceeded

    CoInitialize (NULL);

    cHttp http;
    while (true) {
      if (mPlayer->getSource() != mChangeToChannel) {
        mPlayer->setPlayFrame (mPlayer->changeSource (&http, mChangeToChannel) - mPlayer->getAudFramesFromSec(6));
        changed();
        }
      if (!mPlayer->load (getDeviceContext(), &http, mPlayer->getPlayFrame())) {
        printf ("sleep frame:%d\n", mPlayer->getPlayFrame());
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

    int lastSeqNum = 0;
    while (true) {
      int seqNum;
      int16_t* audioSamples = mPlayer->getAudioSamples (mPlayer->getPlayFrame(), seqNum);
      if (audioSamples && (getVolume() != 80))
        for (auto i = 0; i < 4096; i++)
          audioSamples[i] = (audioSamples[i] * getVolume()) / 80;

      mVidFrame = mPlayer->getVideoFrame (mPlayer->getPlayFrame(), seqNum);
      winAudioPlay ((mPlayer->getPlaying() && audioSamples) ? audioSamples : mSilence, 4096, 1);

      if (mPlayer->getPlaying()) {
        mPlayer->incPlayFrame (1);
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

  // private vars
  iPlayer* mPlayer;

  int mChangeToChannel;
  int mHttpRxBytes;

  bool mShowChan;
  HANDLE mSemaphore;
  int16_t* mSilence;

  int mVidId;
  cVidFrame* mVidFrame;
  ID2D1Bitmap* mBitmap;
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
  //FreeConsole();
#endif

  cHlsRadioWindow hlsRadioWindow;
  hlsRadioWindow.run (L"hlsRadioWindow", 480, 272, argc >= 2 ? _wtoi (argv[1]) : 4);
  }
//}}}
