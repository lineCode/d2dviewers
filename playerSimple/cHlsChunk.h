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
  cHlsChunk() : mSeqNum(0), mBitrate(0), mFramesLoaded(0),
                mChans(0), mSampleRate(0), mDecoder(0), mPower(nullptr), mAudio(nullptr) {}
  //}}}
  //{{{
  ~cHlsChunk() {

    if (mPower)
      vPortFree (mPower);
    if (mAudio)
      vPortFree (mAudio);
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

  // gets
  //{{{
  int getSeqNum() {
    return mSeqNum;
    }
  //}}}
  //{{{
  int getBitrate() {
    return mBitrate;
    }
  //}}}
  //{{{
  int getFramesLoaded() {
    return mFramesLoaded;
    }
  //}}}
  //{{{
  uint8_t* getAudioPower (int frame) {
    return mPower ? mPower + (frame * 2) : nullptr;
    }
  //}}}
  //{{{
  int16_t* getAudioSamples (int frame) {
    return mAudio + (frame * kSamplesPerFrame * mChans);
    }
  //}}}

  //{{{
  bool load (cRadioChan* radioChan, int seqNum, int bitrate) {

    bool ok = false;
    mSeqNum = seqNum;
    mBitrate = bitrate;
    mFramesLoaded = 0;

    //{{{  init decoder
    mDecoder = NeAACDecOpen();
    NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mDecoder);
    config->outputFormat = FAAD_FMT_16BIT;
    NeAACDecSetConfiguration (mDecoder, config);
    //}}}
    if (!mPower)
      mPower = (uint8_t*)pvPortMalloc (kFramesPerChunk * 2);
    if (!mAudio)
      mAudio = (int16_t*)pvPortMalloc (kFramesPerChunk * kSamplesPerFrame * kChans * kBytesPerSample);

    cHttp aacHttp;
    auto response = aacHttp.get (radioChan->getHost(), radioChan->getPath (seqNum, mBitrate));
    if (response == 200) {
      auto loadPtr = aacHttp.getContent();
      auto loadEnd = packTsBuffer (aacHttp.getContent(), aacHttp.getContentEnd());

      // init decoder
      NeAACDecInit (mDecoder, loadPtr, 2048, &mSampleRate, &mChans);

      // aac HE has double size frames, treat as two normal
      int framesPerAacFrame = (mBitrate <= 48000) ? 2 : 1;
      int samplesPerAacFrame = 1024 * framesPerAacFrame;

      NeAACDecFrameInfo frameInfo;
      int16_t* buffer = mAudio;
      NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * kChans * kBytesPerSample);

      uint8_t* powerPtr = mPower;
      while (loadPtr < loadEnd) {
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * kChans * kBytesPerSample);
        loadPtr += frameInfo.bytesconsumed;
        for (int i = 0; i < framesPerAacFrame; i++) {
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

      //printf ("cHlsChunk::loaded %d %d %d %d %d\n", samplesPerAacFrame, mBitrate, mSampleRate, mChans, mSeqNum);
      ok = true;
      }
    else {
      mSeqNum = 0;
      printf ("cHlsChunk::load failed%d\n", response);
      }

    NeAACDecClose (mDecoder);
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
  //{{{  const
  static const int kSamplesRate = 48000;
  static const int kSamplesPerFrame = 1024;
  static const int kFramesPerChunk = 300; // actually 150 for 48k aac he, 300 for 320k 128k 300 frames

  const int kChans = 2;
  const int kBytesPerSample = 2;
  //}}}
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

  // vars
  int mSeqNum;
  int mBitrate;
  int mFramesLoaded;

  unsigned long mSampleRate;
  uint8_t mChans;

  NeAACDecHandle mDecoder;
  uint8_t* mPower;
  int16_t* mAudio;
  };
