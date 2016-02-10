// cYuvFrame.h
#pragma once

static int videoId = 0;

class cYuvFrame  {
public:
  cYuvFrame() : mId(0), mWidth(0), mHeight(0), mYStride(0), mUVStride(0),
                mYbuf(nullptr), mUbuf(nullptr), mVbuf(nullptr) {}

  //{{{
  void set (uint8_t** yuv, int* strides, int width, int height) {

    mId = videoId++;

    mWidth = width;
    mHeight = height;

    mYStride = strides[0];
    mUVStride = strides[1];

    if (mYbuf)
      free (mYbuf);
    mYbuf = (uint8_t*)malloc (height * mYStride);
    memcpy (mYbuf, yuv[0], height * mYStride);

    if (mUbuf)
      free (mUbuf);
    mUbuf = (uint8_t*)malloc ((height/2) * mUVStride);
    memcpy (mUbuf, yuv[1], (height/2) * mUVStride);

    if (mVbuf)
      free (mVbuf);
    mVbuf = (uint8_t*)malloc ((height/2) * mUVStride);
    memcpy (mVbuf, yuv[2], (height/2) * mUVStride);
    }
  //}}}
  //{{{
  void freeResources() {

    mId = 0;
    mWidth = 0;
    mHeight = 0;

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

  int mId;
  int mWidth;
  int mHeight;
  int mYStride;
  int mUVStride;
  uint8_t* mYbuf;
  uint8_t* mUbuf;
  uint8_t* mVbuf;
  };
