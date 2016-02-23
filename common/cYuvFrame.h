// cYuvFrame.h
#pragma once

class cYuvFrame  {
public:
  //{{{
  void set (int64_t pts, uint8_t** yuv, int* strides, int width, int height, int len, int pictType) {

    bool sizeChanged = (mWidth != width) || (mHeight != height) || (mYStride != strides[0]) || (mUVStride != strides[1]);
    mWidth = width;
    mHeight = height;
    mLen = len;
    mPictType = pictType;

    mYStride = strides[0];
    mUVStride = strides[1];

    if (sizeChanged && mYbufUnaligned) {
      free (mYbufUnaligned);
      mYbufUnaligned = nullptr;
      }
    if (!mYbufUnaligned)
      mYbufUnaligned = (uint8_t*)malloc ((height * mYStride) + 15);
    mYbuf = (uint8_t*)(((size_t)(mYbufUnaligned)+15) & ~0xf);
    memcpy (mYbuf, yuv[0], height * mYStride);

    if (sizeChanged && mUbufUnaligned) {
      free (mUbufUnaligned);
      mUbufUnaligned = nullptr;
      }
    if (!mUbufUnaligned)
      mUbufUnaligned = (uint8_t*)malloc (((height/2) * mUVStride) + 15);
    mUbuf = (uint8_t*)(((size_t)(mUbufUnaligned)+15) & ~0xf);
    memcpy (mUbuf, yuv[1], (height/2) * mUVStride);

    if (sizeChanged && mVbufUnaligned) {
      free (mVbufUnaligned);
      mVbufUnaligned = nullptr;
      }
    if (!mVbufUnaligned)
      mVbufUnaligned = (uint8_t*)malloc (((height/2) * mUVStride) + 15);
    mVbuf = (uint8_t*)(((size_t)(mVbufUnaligned)+15) & ~0xf);
    memcpy (mVbuf, yuv[2], (height/2) * mUVStride);

    mPts = pts;
    }
  //}}}
  //{{{
  void freeResources() {

    mPts = 0;
    mWidth = 0;
    mHeight = 0;
    mLen = 0;

    mYStride = 0;
    mUVStride = 0;

    if (mYbufUnaligned)
      free (mYbufUnaligned);
    mYbufUnaligned = nullptr;
    mYbuf = nullptr;

    if (mUbufUnaligned)
      free (mUbufUnaligned);
    mUbufUnaligned = nullptr;
    mUbuf = nullptr;

    if (mVbufUnaligned)
      free (mVbufUnaligned);
    mVbufUnaligned = nullptr;
    mVbuf = nullptr;
    }
  //}}}
  //{{{
  void invalidate() {

    mPts = 0;
    mLen = 0;
    }
  //}}}

  int64_t mPts = 0;
  int mWidth = 0;
  int mHeight = 0;
  int mLen = 0;
  int mPictType = 0;

  int mYStride = 0;
  int mUVStride= 0;

  uint8_t* mYbuf = nullptr;
  uint8_t* mUbuf = nullptr;
  uint8_t* mVbuf = nullptr;

private:
  uint8_t* mYbufUnaligned = nullptr;
  uint8_t* mUbufUnaligned = nullptr;
  uint8_t* mVbufUnaligned = nullptr;
  };
