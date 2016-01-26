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
  cHlsChunk() : mSeqNum(0), mLoaded(0), mChans(0), mSampleRate(0), mPower(nullptr), mAudio(nullptr) {}
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
  static void closeDecoder() {
    NeAACDecClose (mDecoder);
    }
  //}}}
  //{{{
  static int getNumFrames() {
    return kAacFramesPerChunk;
    }
  //}}}
  //{{{
  static int getSamplesPerFrame() {
    return mSamplesPerFrame;
    }
  //}}}
  //{{{
  static int getFrameSeqNum (int frame) {
    return frame / kAacFramesPerChunk;
    }
  //}}}
  //{{{
  static bool getLoading() {
    return mLoading;
    }
  //}}}

  // gets
  //{{{
  int getFrame() {
    return mSeqNum * kAacFramesPerChunk;
    }
  //}}}
  //{{{
  int getSeqNum() {
    return mSeqNum;
    }
  //}}}
  //{{{
  int16_t* getAudioSamples (int frame) {
    return mAudio + (frame * mSamplesPerFrame * mChans);
    }
  //}}}
  //{{{
  void getAudioPower (int frame, uint8_t** powerPtr) {

    *powerPtr = mPower ? mPower + (frame * 2) : nullptr;

    }
  //}}}

  //{{{
  bool load (cRadioChan* radioChan, int seqNum) {

    if (!mDecoder) {
      //{{{  init decoder
      mDecoder = NeAACDecOpen();
      NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mDecoder);
      config->outputFormat = FAAD_FMT_16BIT;
      NeAACDecSetConfiguration (mDecoder, config);
      }
      //}}}

    bool ok = false;
    mLoaded = 0;
    mLoading = true;
    mSamplesPerFrame = (radioChan->getBitrate() <= 48000) ? 2048 : 1024;

    if (!mPower)
      mPower = (uint8_t*)malloc (kAacFramesPerChunk * kChans);
    if (!mAudio)
      mAudio = (int16_t*)malloc (kAacFramesPerChunk * mSamplesPerFrame * kChans * kBytesPerSample);

    cHttp aacHttp;
    auto response = aacHttp.get (radioChan->getHost(), radioChan->getPath (seqNum));
    if (response == 200) {
      mSeqNum = seqNum;

      auto loadPtr = aacHttp.getContent();
      auto loadEnd = packTsBuffer (aacHttp.getContent(), aacHttp.getContentEnd());

      // init decoder
      NeAACDecInit (mDecoder, loadPtr, 2048, &mSampleRate, &mChans);

      NeAACDecFrameInfo frameInfo;
      int16_t* buffer = mAudio;
      NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), mSamplesPerFrame * kChans * kBytesPerSample);

      uint8_t* powerPtr = mPower;
      while (loadPtr < loadEnd) {
        NeAACDecFrameInfo frameInfo;
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), mSamplesPerFrame * kChans * kBytesPerSample);
        loadPtr += frameInfo.bytesconsumed;
        //{{{  calc left, right power
        int valueL = 0;
        int valueR = 0;
        for (int j = 0; j < mSamplesPerFrame; j++) {
          short sample = (*buffer++) >> 4;
          valueL += sample * sample;
          sample = (*buffer++) >> 4;
          valueR += sample * sample;
          }

        uint8_t leftPix = (uint8_t)sqrt(valueL / (mSamplesPerFrame * 32.0f));
        *powerPtr++ = (272/2) - leftPix;
        *powerPtr++ = leftPix + (uint8_t)sqrt(valueR / (mSamplesPerFrame * 32.0f));
        //}}}
        mLoaded++;
        }

      printf ("cHlsChunk:load %d %d %d %d\n", mSamplesPerFrame, mSampleRate, mChans, mSeqNum);
      ok = true;
      }
    else
      printf ("cHlsChunk:load %d\n", response);

    mLoading = false;
    return ok;
    }
  //}}}
  //{{{
  bool contains (int frame, int& seqNum, int& frameInChunk) {

    if ((frame >= getFrame()) && (frame < getFrame() + mLoaded)) {
      seqNum = mSeqNum;
      frameInChunk = frame - getFrame();
      return true;
      }
    return false;
    }
  //}}}
  //{{{
  void invalidate() {

    mSeqNum = 0;
    mLoaded = 0;
    }
  //}}}

private:
  // const
  const int kChans = 2;
  const int kBytesPerSample = 2;

  static const int kAacFramesPerChunk = 300;
  static int mSamplesPerFrame;
  static bool mLoading;
  static NeAACDecHandle mDecoder;

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

  //{{{  vars
  int mSeqNum;
  int mLoaded;

  unsigned long mSampleRate;
  uint8_t mChans;

  uint8_t* mPower;
  int16_t* mAudio;
  //}}}
  };

// cHlsChunk static var init
NeAACDecHandle cHlsChunk::mDecoder = 0;
bool cHlsChunk::mLoading = false;
int cHlsChunk::mSamplesPerFrame = 0;
