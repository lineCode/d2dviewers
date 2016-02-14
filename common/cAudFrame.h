// cYuvFrame.h
#pragma once

class cAudFrame  {
public:
  cAudFrame() : mPts(0), mNumSamples(0), mSamples(nullptr), mPowerLeft(0), mPowerRight(0) {}
  //{{{
  ~cAudFrame() {
    if (mSamples)
      free (mSamples);
    }
  //}}}

  //{{{
  void set (int64_t pts, int channels, int numSamples) {

    mPts = pts;

    if ((numSamples != mNumSamples) && mSamples) {
      free (mSamples);
      mSamples = nullptr;
      }

    if (!mSamples)
      mSamples = (int16_t*)malloc (channels * numSamples* 2);
    }
  //}}}
  //{{{
  void freeResources() {

    mPts = 0;
    mNumSamples = 0;

    if (mSamples)
      free (mSamples);
    mSamples = nullptr;
    }
  //}}}

  int64_t mPts;
  int mNumSamples;
  int16_t* mSamples;
  float mPowerLeft;
  float mPowerRight;
  };
