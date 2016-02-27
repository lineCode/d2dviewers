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

    if (sizeChanged && mYbuf)
      free (mYbuf);
    mYbuf = (uint8_t*)_mm_malloc (height * mYStride, 128);
    memcpy (mYbuf, yuv[0], height * mYStride);

    if (sizeChanged && mUbuf)
      free (mUbuf);
    mUbuf = (uint8_t*)_mm_malloc ((height/2) * mUVStride, 128);
    memcpy (mUbuf, yuv[1], (height/2) * mUVStride);

    if (sizeChanged && mVbuf)
      free (mVbuf);
    mVbuf = (uint8_t*)_mm_malloc ((height/2) * mUVStride, 128);
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

    if (mYbuf)
      free (mYbuf);
    mYbuf = nullptr;

    if (mUbuf)
      free (mUbuf);
    mUbuf = nullptr;

    if (mVbuf)
      free (mVbuf);
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
  };
