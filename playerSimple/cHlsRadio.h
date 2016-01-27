// cHlsRadio.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
#include "cRadioChan.h"
#include "cHlsChunk.h"
//}}}

class cHlsRadio {
public:
  #define NUM_CHUNKS 3
  //{{{
  cHlsRadio() : mBaseSeqNum(0), mBaseFrame(0), mLoading(0) {

    mSilence = (int16_t*) pvPortMalloc (cHlsChunk::getSamplesPerFrame()*2*2);
    for (auto i = 0; i < cHlsChunk::getSamplesPerFrame()*2; i++)
      mSilence[i] = 0;
    }
  //}}}
  //{{{
  ~cHlsRadio() {
    vPortFree (mSilence);
    }
  //}}}

  //{{{
  int getBitrate() {
    return mBitrate;
    }
  //}}}
  //{{{
  int getLoading() {
    return mLoading;
    }
  //}}}
  //{{{
  float getFramesPerSec() {
    return cHlsChunk::getFramesPerSec();
    }
  //}}}
  //{{{
  int getSamplesPerFrame() {
    return cHlsChunk::getSamplesPerFrame();
    }
  //}}}
  //{{{
  const char* getDebugStr() {

    sprintf (mDebugStr, "%d:%d:%d\n",
             mChunks[0].getSeqNum() ? mChunks[0].getSeqNum() - mBaseSeqNum : 9999,
             mChunks[1].getSeqNum() ? mChunks[1].getSeqNum() - mBaseSeqNum : 9999,
             mChunks[2].getSeqNum() ? mChunks[2].getSeqNum() - mBaseSeqNum : 9999);
    return mDebugStr;
    }
  //}}}
  //{{{
  const char* getChanDisplayName() {

    return mRadioChan.getChanDisplayName();
    }
  //}}}
  //{{{
  int16_t* getPlay (int frame, bool& playing, int& seqNum) {
  // return audio buffer for frame, silence if not loaded or not playing

    int chunk;
    int frameInChunk;
    playing &= findFrame (frame, seqNum, chunk, frameInChunk);

    return playing ? mChunks[chunk].getAudioSamples (frameInChunk) : mSilence;
    }
  //}}}
  //{{{
  uint8_t* getPower (int frame, int& frames) {
  // return pointer to frame power org,len uint8_t pairs
  // frames = number of valid frames

    int chunk;
    int seqNum;
    int frameInChunk;
    if (findFrame (frame, seqNum, chunk, frameInChunk)) {
      frames = cHlsChunk::getFramesPerChunk() - frameInChunk;
      return mChunks[chunk].getAudioPower (frameInChunk);
      }
    else {
      chunk = 0;
      frames = 0;
      return nullptr;
      }
    }
  //}}}

  //{{{
  int setChanBitrate (int chan, int bitrate) {

    mBitrate = bitrate;

    printf ("cHlsChunks::setChan %d %d\n", chan, bitrate);
    mBaseSeqNum = mRadioChan.setChan (chan, bitrate);

    const char* dateTime = mRadioChan.getDateTime();
    int hour = ((dateTime[11] - '0') * 10) + (dateTime[12] - '0');
    int min = ((dateTime[14] - '0') * 10) + (dateTime[15] - '0');
    int sec = ((dateTime[17] - '0') * 10) + (dateTime[18] - '0');
    mBaseFrame = (((hour*60*60) + (min*60) + sec) * cHlsChunk::getFramesPerChunk() * 10) / 64;
    printf ("- baseFrame:%d baseSeqNum:%d dateTime:%s\n", mBaseFrame, mBaseSeqNum, mRadioChan.getDateTime());

    invalidateChunks();
    return mBaseFrame;
    }
  //}}}
  //{{{
  void setBitrate (int bitrate) {

    mBitrate = bitrate;
    }
  //}}}

  //{{{
  bool load (int frame) {
  // return false if load failure, usually 404

    bool ok = false;

    mLoading = 0;
    int chunk;
    int seqNum = getSeqNumFromFrame (frame);
    if (!findSeqNumChunk (seqNum, mBitrate, 0, chunk)) {
      mLoading++;
      ok &= mChunks[chunk].load (&mRadioChan, seqNum, mBitrate);
      }

    for (auto i = 1; i <= NUM_CHUNKS/2; i++) {
      if (!findSeqNumChunk (seqNum, mBitrate, i, chunk)) {
        mLoading++;
        ok &= mChunks[chunk].load (&mRadioChan, seqNum+i, mBitrate);
        }
      if (!findSeqNumChunk (seqNum, mBitrate, -i, chunk)) {
        mLoading++;
        ok &= mChunks[chunk].load (&mRadioChan, seqNum-i, mBitrate);
        }
      }
    mLoading = 0;

    return ok;
    }
  //}}}

private:
  //{{{
  int getSeqNumFromFrame (int frame) {

    int r = frame - mBaseFrame;
    if (r >= 0)
      return mBaseSeqNum + (r / cHlsChunk::getFramesPerChunk());
    else
      return mBaseSeqNum - 1 - ((-r-1)/ cHlsChunk::getFramesPerChunk());
    }
  //}}}
  //{{{
  int getFrameInChunkFromFrame (int frame) {

    int r = (frame - mBaseFrame) % cHlsChunk::getFramesPerChunk();
    return r < 0 ? r + cHlsChunk::getFramesPerChunk() : r;
    }
  //}}}

  //{{{
  bool findFrame (int frame, int& seqNum, int& chunk, int& frameInChunk) {

    auto findSeqNum = getSeqNumFromFrame (frame);
    for (auto i = 0; i < NUM_CHUNKS; i++) {
      if ((mChunks[i].getSeqNum() != 0) && (findSeqNum == mChunks[i].getSeqNum())) {
        auto findFrameInChunk = getFrameInChunkFromFrame (frame);
        if ((mChunks[i].getFramesLoaded() > 0) && (findFrameInChunk < mChunks[i].getFramesLoaded())) {
          seqNum = findSeqNum;
          chunk = i;
          frameInChunk = findFrameInChunk;
          return true;
          }
        }
      }

    chunk = 0;
    seqNum = 0;
    frameInChunk = 0;
    return false;
    }
  //}}}
  //{{{
  bool findSeqNumChunk (int seqNum, int bitrate, int offset, int& chunk) {
  // return true if match found
  // - if not chunk = best reuse
  // - reuse same seqNum chunk if diff bitrate

    // look for matching chunk
    chunk = 0;
    while (chunk < NUM_CHUNKS) {
      if (seqNum + offset == mChunks[chunk].getSeqNum())
        return true;
        //return bitrate != mChunks[chunk].getBitrate();
      chunk++;
      }

    // look for stale chunk
    chunk = 0;
    while (chunk < NUM_CHUNKS) {
      if ((mChunks[chunk].getSeqNum() < seqNum-(NUM_CHUNKS/2)) || (mChunks[chunk].getSeqNum() > seqNum+(NUM_CHUNKS/2)))
        return false;
      chunk++;
      }

    printf ("cHlsRadio::findSeqNumChunk problem %d", seqNum);
    chunk = 0;
    return false;
    }
  //}}}
  //{{{
  void invalidateChunks() {

    for (auto i = 0; i < NUM_CHUNKS; i++)
      mChunks[i].invalidate();
    }
  //}}}

  cRadioChan mRadioChan;
  int mBaseSeqNum;
  int mBaseFrame;
  int mBitrate;
  int mLoading;
  char mDebugStr [20];
  cHlsChunk mChunks [NUM_CHUNKS];
  int16_t* mSilence;
  };
