// cHlsChunk.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
#include "cRadioChan.h"
//}}}

class cHlsChunk {
public:
  //{{{
  cHlsChunk() : mSeqNum(0), mFramesLoaded(0),
                mChans(0), mSampleRate(0), mAacSamplesPerAacFrame(0),
                mPower(nullptr), mAudio(nullptr) {}
  //}}}
  //{{{
  ~cHlsChunk() {

    if (mPower)
      free (mPower);
    if (mAudio)
      free (mAudio);
    }
  //}}}

  // static members
  //{{{
  static float getFramesPerSec() {
    return float(kSamplesRate) / (float)kSamplesPerFrame;
    }
  //}}}
  //{{{
  static int getSamplesPerFrame() {
    return kSamplesPerFrame;
    }
  //}}}
  //{{{
  static int getFramesPerChunk() {
    return kFramesPerChunk;
    }
  //}}}
  //{{{
  static bool getLoading() {
    return mLoading;
    }
  //}}}
  //{{{
  static void closeDecoder() {
    NeAACDecClose (mDecoder);
    }
  //}}}

  // gets
  //{{{
  int getSeqNum() {
    return mSeqNum;
    }
  //}}}
  //{{{
  int getFramesLoaded() {
    return mFramesLoaded;
    }
  //}}}
  //{{{
  int16_t* getAudioSamples (int frame) {
    return mAudio + (frame * kSamplesPerFrame * mChans);
    }
  //}}}
  //{{{
  void getAudioPower (int frame, uint8_t** powerPtr) {

    *powerPtr = mPower ? mPower + (frame * 2) : nullptr;

    }
  //}}}

  //{{{
  bool load (cRadioChan* radioChan, int seqNum) {

    //printf ("cHlsChunk::load %d\n", seqNum);
    mSeqNum = seqNum;
    mFramesLoaded = 0;
    bool ok = false;
    mLoading = true;
    mAacSamplesPerAacFrame = (radioChan->getBitrate() <= 48000) ? 2048 : 1024;

    if (!mDecoder) {
      //{{{  init decoder
      mDecoder = NeAACDecOpen();
      NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mDecoder);
      config->outputFormat = FAAD_FMT_16BIT;
      NeAACDecSetConfiguration (mDecoder, config);
      }
      //}}}
    if (!mPower)
      mPower = (uint8_t*)malloc (kFramesPerChunk * kBytesPerSample);
    if (!mAudio)
      mAudio = (int16_t*)malloc (kFramesPerChunk * kSamplesPerFrame * kChans * kBytesPerSample);

    cHttp aacHttp;
    auto response = aacHttp.get (radioChan->getHost(), radioChan->getPath (seqNum));
    if (response == 200) {
      auto loadPtr = aacHttp.getContent();
      auto loadEnd = packTsBuffer (aacHttp.getContent(), aacHttp.getContentEnd());

      // init decoder
      NeAACDecInit (mDecoder, loadPtr, (unsigned long)(aacHttp.getContentEnd() - loadPtr), &mSampleRate, &mChans);

      NeAACDecFrameInfo frameInfo;
      int16_t* buffer = mAudio;
      NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, (unsigned long)(aacHttp.getContentEnd() - loadPtr),
                       (void**)(&buffer), mAacSamplesPerAacFrame * kChans * kBytesPerSample);

      uint8_t* powerPtr = mPower;
      while (loadPtr < loadEnd) {
        NeAACDecFrameInfo frameInfo;
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, (unsigned long)(aacHttp.getContentEnd() - loadPtr),
                         (void**)(&buffer), mAacSamplesPerAacFrame * kChans * kBytesPerSample);
        loadPtr += frameInfo.bytesconsumed;

        // aac HE has double size frames, treat as two normal
        for (auto i = 0; i < mAacSamplesPerAacFrame / kSamplesPerFrame; i++) {
          //{{{  calc left, right power
          int valueL = 0;
          int valueR = 0;
          for (int j = 0; j < kSamplesPerFrame; j++) {
            short sample = (*buffer++) >> 4;
            valueL += sample * sample;
            sample = (*buffer++) >> 4;
            valueR += sample * sample;
            }

          uint8_t leftPix = (uint8_t)sqrt(valueL / (kSamplesPerFrame * 32.0f));
          *powerPtr++ = (272/2) - leftPix;
          *powerPtr++ = leftPix + (uint8_t)sqrt(valueR / (kSamplesPerFrame * 32.0f));
          mFramesLoaded++;
          }
          //}}}
        }

      //printf ("cHlsChunk::loaded %d %d %d %d\n", mAacSamplesPerAacFrame, mSampleRate, mChans, mSeqNum);
      ok = true;
      }
    else {
      mSeqNum = 0;
      printf ("cHlsChunk::load failed%d\n", response);
      }

    mLoading = false;
    return ok;
    }
  //}}}
  //{{{
  void invalidate() {

    mSeqNum = 0;
    mFramesLoaded = 0;
    }
  //}}}

private:
  //{{{
  uint8_t* packTsBuffer (uint8_t* ptr, uint8_t* endPtr) {
  // pack transportStream down to aac frames in same buffer

    auto toPtr = ptr;
    while ((ptr < endPtr) && (*ptr++ == 0x47)) {
      auto payStart = *ptr & 0x40;
      auto pid = ((*ptr & 0x1F) << 8) | *(ptr+1);
      auto headerBytes = (*(ptr+2) & 0x20) ? (4 + *(ptr+3)) : 3;
      ptr += headerBytes;
      auto tsFrameBytesLeft = 187 - headerBytes;
      if (pid == 34) {
        if (payStart && !(*ptr) && !(*(ptr+1)) && (*(ptr+2) == 1) && (*(ptr+3) == 0xC0)) {
          int pesHeaderBytes = 9 + *(ptr+8);
          ptr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          }
        memcpy (toPtr, ptr, tsFrameBytesLeft);
        toPtr += tsFrameBytesLeft;
        }
      ptr += tsFrameBytesLeft;
      }

    return toPtr;
    }
  //}}}

  //{{{  const
  const int kChans = 2;
  const int kBytesPerSample = 2;
  static const int kSamplesRate = 48000;
  static const int kSamplesPerFrame = 1024;
  static const int kFramesPerChunk = 300; // actually 150 for 48k aac he, 300 for 320k 128k 300 frames
  //}}}
  //{{{  static vars
  static bool mLoading;
  static NeAACDecHandle mDecoder;
  //}}}
  //{{{  vars
  int mSeqNum;
  int mFramesLoaded;

  unsigned long mSampleRate;
  uint8_t mChans;
  int mAacSamplesPerAacFrame;

  uint8_t* mPower;
  int16_t* mAudio;
  //}}}
  };

// cHlsChunk static var init
NeAACDecHandle cHlsChunk::mDecoder = 0;
bool cHlsChunk::mLoading = false;
