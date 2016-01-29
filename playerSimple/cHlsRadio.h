// cHlsRadio.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
#include "cRadioChan.h"
#include "cHlsChunk.h"
//}}}

class cHlsRadio : public cRadioChan {
public:
  #define NUM_CHUNKS 3
  cHlsRadio() : mBaseFrame(0), mLoading(0), mJumped(false) {}
  ~cHlsRadio() {}

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
  const char* getInfoStr (int frame) {

    int secsSinceMidnight = int (frame / getFramesPerSecond());
    int secs = secsSinceMidnight % 60;
    int mins = (secsSinceMidnight / 60) % 60;
    int hours = secsSinceMidnight / (60*60);

    sprintf (mInfoStr, "%d:%02d:%02d %s %dk %d:%d:%d",
             hours, mins, secs, getChanName(), getBitrate()/1000,
             mChunks[0].getSeqNum() ? mChunks[0].getSeqNum() - getBaseSeqNum() : 9999,
             mChunks[1].getSeqNum() ? mChunks[1].getSeqNum() - getBaseSeqNum() : 9999,
             mChunks[2].getSeqNum() ? mChunks[2].getSeqNum() - getBaseSeqNum() : 9999);

    return mInfoStr;
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
      frames = getFramesPerChunk() - frameInChunk;
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
  int16_t* getAudioSamples (int frame, int& seqNum) {
  // return audio buffer for frame

    int chunk;
    int frameInChunk;
    return findFrame (frame, seqNum, chunk, frameInChunk) ? mChunks[chunk].getAudioSamples (frameInChunk) : nullptr;
    }
  //}}}

  //{{{
  int changeChan (int chan) {

    printf ("cHlsChunks::setChan %d\n", chan);
    setChan (chan);
    mBitrate = getLowBitrate();

    int hour = ((getDateTime()[11] - '0') * 10) + (getDateTime()[12] - '0');
    int min =  ((getDateTime()[14] - '0') * 10) + (getDateTime()[15] - '0');
    int sec =  ((getDateTime()[17] - '0') * 10) + (getDateTime()[18] - '0');
    int secsSinceMidnight = (hour * 60 * 60) + (min * 60) + sec;
    mBaseFrame = getFramesFromSec (secsSinceMidnight);
    printf ("cHlsRadio::changeChan- baseSeqNum:%d dateTime:%s %dh %dm %ds %d baseFrame:%d\n",
            getBaseSeqNum(), getDateTime(), hour, min, sec, secsSinceMidnight, mBaseFrame);

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
  void setBitrateStrategy (bool jumped) {

    mJumped = jumped;
    if (jumped)
      // jumping around, low quality
      setBitrate (getLowBitrate());
    else if (getBitrate() == getLowBitrate())
      // normal play, better quality
      setBitrate (getMidBitrate());
    else if (getBitrate() == getMidBitrate())
      // normal play, much better quality
      setBitrate (getHighBitrate());
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
      ok &= mChunks[chunk].load (this, seqNum, mBitrate);
      }

    if (!mJumped) {
      // load chunks before and after
      for (auto i = 1; i <= NUM_CHUNKS/2; i++) {
        if (!findSeqNumChunk (seqNum, mBitrate, i, chunk)) {
          mLoading++;
          ok &= mChunks[chunk].load (this, seqNum+i, mBitrate);
          }
        if (!findSeqNumChunk (seqNum, mBitrate, -i, chunk)) {
          mLoading++;
          ok &= mChunks[chunk].load (this, seqNum-i, mBitrate);
          }
        }
      }
    mJumped = false;
    mLoading = 0;

    return ok;
    }
  //}}}

private:
  //{{{
  int getSeqNumFromFrame (int frame) {

    int r = frame - mBaseFrame;
    if (r >= 0)
      return getBaseSeqNum() + (r / getFramesPerChunk());
    else
      return getBaseSeqNum() - 1 - ((-r-1)/ getFramesPerChunk());
    }
  //}}}
  //{{{
  int getFrameInChunkFromFrame (int frame) {

    int r = (frame - mBaseFrame) % getFramesPerChunk();
    return r < 0 ? r + getFramesPerChunk() : r;
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

  int mBaseFrame;
  int mBitrate;
  int mLoading;
  bool mJumped;
  char mInfoStr [40];
  cHlsChunk mChunks [NUM_CHUNKS];
  };
