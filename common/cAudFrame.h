// cYuvFrame.h
#pragma once
#define maxSampleBytes 1152*2*6

class cAudFrame  {
public:
  //{{{
  cAudFrame() {
  #ifdef WIN32
    mSamples = (int16_t*)malloc (maxSampleBytes);
  #endif
    }
  //}}}
  //{{{
  ~cAudFrame() {
    if (mSamples)
      free (mSamples);
    }
  //}}}

  //{{{
  void set (int64_t pts, int channels, int sampleRate, int numSamples) {

    mPts = pts;
    mChannels = channels;
    mNumSamples = numSamples;
    mSampleRate = sampleRate;

  #ifdef WIN32
    mNumSampleBytes = mChannels * mNumSamples* 2;
    if (mNumSampleBytes > maxSampleBytes)
      printf ("cAudFrame - too many samples\n");
  #else
    if ((mChannels * mNumSamples * 2 != mNumSampleBytes) && mSamples) {
      free (mSamples);
      mSamples = nullptr;
      mNumSampleBytes = 0;
      }

    if (!mSamples) {
      mNumSampleBytes = mChannels * mNumSamples * 2;
      mSamples = (int16_t*)malloc (mNumSampleBytes);
      }
  #endif
    }
  //}}}
  //{{{
  void freeResources() {

    mPts = 0;
    mChannels = 0;
    mNumSamples = 0;
    mSampleRate = 0;

    mNumSampleBytes = 0;
    if (mSamples)
      free (mSamples);
    mSamples = nullptr;
    }
  //}}}
  //{{{
  void invalidate() {

    mPts = 0;
    mChannels = 0;
    mNumSamples = 0;
    mSampleRate = 0;
    }
  //}}}

  int64_t mPts = 0;

  int mChannels = 0;
  int mNumSamples = 0;
  int mSampleRate = 48000;

  int mNumSampleBytes = 0;
  int16_t* mSamples = nullptr;
  float mPower[6] = { 0,0,0,0,0,0 };
  };
