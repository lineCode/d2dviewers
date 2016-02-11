// cYuvFrame.h
#pragma once

static int videoId = 0;

class cYuvFrame  {
public:
  cYuvFrame() : mId(0), mWidth(0), mHeight(0), mYStride(0), mUVStride(0),
                mYbuf(nullptr), mUbuf(nullptr), mVbuf(nullptr), mYbufRaw(nullptr), mUbufRaw(nullptr), mVbufRaw(nullptr) {}

  //{{{
  void set (uint8_t** yuv, int* strides, int width, int height) {

    mId = videoId++;

    mWidth = width;
    mHeight = height;

    mYStride = strides[0];
    mUVStride = strides[1];

    if (mYbufRaw)
      free (mYbufRaw);
    mYbufRaw = (uint8_t*)malloc ((height * mYStride) + 15);
    mYbuf = (uint8_t*)(((size_t)(mYbufRaw)+15) & ~0xf);
    memcpy (mYbuf, yuv[0], height * mYStride);

    if (mUbufRaw)
      free (mUbufRaw);
    mUbufRaw = (uint8_t*)malloc (((height/2) * mUVStride) + 15);
    mUbuf = (uint8_t*)(((size_t)(mUbufRaw)+15) & ~0xf);
    memcpy (mUbuf, yuv[1], (height/2) * mUVStride);

    if (mVbufRaw)
      free (mVbufRaw);
    mVbufRaw = (uint8_t*)malloc (((height/2) * mUVStride) + 15);
    mVbuf = (uint8_t*)(((size_t)(mVbufRaw)+15) & ~0xf);
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

private:
  uint8_t* mYbufRaw;
  uint8_t* mUbufRaw;
  uint8_t* mVbufRaw;
  };
