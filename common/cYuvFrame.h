// cYuvFrame.h
#pragma once

static int videoId = 0;

class cYuvFrame  {
public:
  cYuvFrame(): mYbuf(nullptr), mUbuf(nullptr),mVbuf(nullptr),
               mYStride(0), mUVStride(0), mWidth(0), mHeight(0), mId(0) {}

  //{{{
  void set (uint8_t** yuv, int* strides, int width, int height) {

    mYStride = strides[0];
    mUVStride = strides[1];

    mWidth = width;
    mHeight = height;
    mId = videoId++;

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

  uint8_t* mYbuf;
  uint8_t* mUbuf;
  uint8_t* mVbuf;

  int mYStride;
  int mUVStride;

  int mWidth;
  int mHeight;

  int mId;
  };
