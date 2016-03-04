// mp3window.cpp
//{{{  includes
#include "pch.h"

#include "../common/timer.h"
#include "../common/cD2dWindow.h"

#include "../common/cAudFrame.h"
#include "../common/winAudio.h"

#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
//}}}

#define maxAudFrames 500000

class cAppWindow : public cD2dWindow {
public:
  //{{{
  cAppWindow() {
    mSilence = (int16_t*)malloc (2048*4);
    memset (mSilence, 0, 2048*4);
    }
  //}}}
  ~cAppWindow() {}
  //{{{
  void run (wchar_t* title, int width, int height, wchar_t* wFilename) {

    initialise (title, width, height);

    std::thread ([=]() { loader (wFilename); }).detach();
    std::thread ([=]() { player(); }).detach();

    // loop in windows message pump till quit
    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x00 : break;
      case 0x1B : return true;

      case 0x20 : mPlaying = !mPlaying; break;

      case 0x21 : mPlayAudFrame -= 5 * mAudFramesPerSec; changed(); break;
      case 0x22 : mPlayAudFrame += 5 * mAudFramesPerSec; changed(); break;

      case 0x23 : mPlayAudFrame = mLoadAudFrame - 1; changed(); break;
      case 0x24 : mPlayAudFrame = 0; break;

      case 0x25 : mPlayAudFrame -= 1 * mAudFramesPerSec; changed(); break;
      case 0x27 : mPlayAudFrame += 1 * mAudFramesPerSec; changed(); break;

      case 0x26 : mPlaying = false; mPlayAudFrame -=2; changed(); break;
      case 0x28 : mPlaying = false; mPlayAudFrame +=2; changed(); break;

      default   : printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    mPlayAudFrame -= xInc;

    changed();
    }
  //}}}
  //{{{
  void onMouseUp  (bool right, bool mouseMoved, int x, int y) {

    //if (!mouseMoved)
      //playFrame += x - (getClientF().width/2.0f);
    changed();
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    dc->Clear (ColorF(ColorF::Black));

    uint8_t* power = nullptr;
    int frame = mPlayAudFrame;
    auto frames = 0;
    auto rWave = RectF (0,0,1,0);
    for (int i = 0; i < 10; i++) {
      for (float f = 0.0f; f < getClientF().width; f++) {
        auto rWave = RectF (!(i & 1) ? f : (getClientF().width-f), 0, !(i & 1) ? (f+1.0f) : (getClientF().width-f+1.0f), 0);
        if (mAudFrames[frame]) {
          rWave.top = i * (getClientF().height / 10.0f) + mAudFrames[frame]->mPower[0];
          rWave.bottom = rWave.top + mAudFrames[frame]->mPower[1];
          dc->FillRectangle (rWave, getBlueBrush());
          }
        frame++;
        }
      }

    wchar_t wStr[200];
    swprintf (wStr, 200, L"helloColin %d %d", mLoadAudFrame, mPlayAudFrame);
    dc->DrawText (wStr, (UINT32)wcslen(wStr), getTextFormat(),
                  RectF(0.0f, 0.0f, getClientF().width, getClientF().height), getWhiteBrush());
    }
  //}}}

private:
  //{{{
  void loader (wchar_t* wFilename) {

    auto fileHandle = CreateFile (wFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
      return;
    auto mFileBytes = GetFileSize (fileHandle, NULL);
    auto mapHandle = CreateFileMapping (fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    auto fileBuffer = (BYTE*)MapViewOfFile (mapHandle, FILE_MAP_READ, 0, 0, 0);

    av_register_all();
    AVCodecParserContext* audParser = av_parser_init (AV_CODEC_ID_MP3);
    AVCodec* audCodec = avcodec_find_decoder (AV_CODEC_ID_MP3);
    AVCodecContext*audContext = avcodec_alloc_context3 (audCodec);
    avcodec_open2 (audContext, audCodec, NULL);

    AVPacket avPacket;
    av_init_packet (&avPacket);
    avPacket.data = fileBuffer;
    avPacket.size = 0;

    auto time = startTimer();
    auto pesPtr = fileBuffer;
    auto pesLen = mFileBytes;
    while (pesLen) {
      auto lenUsed = av_parser_parse2 (audParser, audContext, &avPacket.data, &avPacket.size, pesPtr, pesLen, 0, 0, AV_NOPTS_VALUE);
      pesPtr += lenUsed;
      pesLen -= lenUsed;

      if (avPacket.data) {
        auto got = 0;
        auto avFrame = av_frame_alloc();
        auto bytesUsed = avcodec_decode_audio4 (audContext, avFrame, &got, &avPacket);
        if (bytesUsed >= 0) {
          avPacket.data += bytesUsed;
          avPacket.size -= bytesUsed;

          if (got) {
            mSampleRate = avFrame->sample_rate;
            mAudFrames[mLoadAudFrame] = new cAudFrame();
            mAudFrames[mLoadAudFrame]->set (0, avFrame->channels, avFrame->sample_rate, avFrame->nb_samples);
            auto samplePtr = (short*)mAudFrames[mLoadAudFrame]->mSamples;
            auto valueL = 0.0;
            auto valueR = 0.0;
            auto leftPtr = (short*)avFrame->data[0];
            auto rightPtr = (short*)avFrame->data[1];
            for (auto i = 0; i < avFrame->nb_samples; i++) {
              *samplePtr = *leftPtr++;
              valueL += pow (*samplePtr++, 2);
              *samplePtr = *rightPtr++;
              valueR += pow (*samplePtr++, 2);
              }
            mAudFrames[mLoadAudFrame]->mPower[0] = (float)sqrt (valueL) / (avFrame->nb_samples * 2.0f);
            mAudFrames[mLoadAudFrame]->mPower[1] = (float)sqrt (valueR) / (avFrame->nb_samples * 2.0f);
            mLoadAudFrame++;
            changed();
            }
          av_frame_free (&avFrame);
          }
        }
      }

    UnmapViewOfFile (fileBuffer);
    CloseHandle (fileHandle);
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);  // for winAudio

    while (mLoadAudFrame < 8)
      Sleep(10);
    winAudioOpen (mSampleRate, 16, 2);

    mPlayAudFrame = 0;
    while (true) {
      if (mPlaying) {
        winAudioPlay (mAudFrames[mPlayAudFrame]->mSamples, mAudFrames[mPlayAudFrame]->mNumSampleBytes, 1.0f);
        mPlayAudFrame++;
        }
      else
        winAudioPlay (mSilence, 4096, 1.0f);
      changed();
      }

    CoUninitialize();
    }
  //}}}

  int16_t* mSilence;

  bool mPlaying = true;
  int mSampleRate = 0;
  int mPlayAudFrame = 0;
  int mLoadAudFrame = 0;
  int mAudFramesPerSec = 50;
  cAudFrame* mAudFrames[maxAudFrames];
  };

static cAppWindow appWindow;

//{{{
int wmain (int argc, wchar_t* argv[]) {

  printf ("mp3wWindow/n");

  appWindow.run (L"appWindow", 1280, 720, argv[1]);
  }
//}}}
