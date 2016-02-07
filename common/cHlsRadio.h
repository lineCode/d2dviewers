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
  //{{{
  cHlsRadio() : mTuneVol(80), mTuneChan(4), mPlayFrame(0), mPlaying(true), mRxBytes(0),
                mBaseFrame(0), mAudBitrate(0), mLoading(0), mJumped(false) {}
  //}}}
  ~cHlsRadio() {}

  //{{{
  int getAudBitrate() {
    return mAudBitrate;
    }
  //}}}
  //{{{
  int getLoading() {
    return mLoading;
    }
  //}}}
  //{{{
  std::string getInfoStr (int frame) {
    return getChanName (getChan()) + ':' + toString (getAudBitrate()/1000) + "k " + getFrameInfo (frame);
           //+ getChunkNumStr (0) + ':' + getChunkNumStr (1) + ':' + getChunkNumStr (2);
    }
  //}}}
  //{{{
  std::string getFrameInfo (int frame) {

    int secsSinceMidnight = int (frame / getAudFramesPerSecond());
    int secs = secsSinceMidnight % 60;
    int mins = (secsSinceMidnight / 60) % 60;
    int hours = secsSinceMidnight / (60*60);

    return toString (hours) + ':' + toString (mins) + ':' + toString (secs);
    }
  //}}}
  //{{{
  std::string getChunkInfoStr (int chunk) {
    return getChunkNumStr (chunk) + ':' + mChunks[chunk].getInfoStr();
    }
  //}}}
  //{{{
  uint8_t* getPower (int frame, int& frames) {
  // return pointer to frame power org,len uint8_t pairs
  // frames = number of valid frames

    int seqNum;
    int chunk = 0;
    int frameInChunk;
    frames = 0;
    return findAudFrame(frame, seqNum, chunk, frameInChunk) ? mChunks[chunk].getAudioPower (frameInChunk, frames) : nullptr;
    }
  //}}}
  //{{{
  int16_t* getAudioSamples (int frame, int& seqNum) {
  // return audio buffer for frame

    int chunk;
    int frameInChunk;
    return findAudFrame(frame, seqNum, chunk, frameInChunk) ? mChunks[chunk].getAudioSamples (frameInChunk) : nullptr;
    }
  //}}}
  //{{{
  IWICBitmap* getVideoFrame (int frame, int seqNum) {
  // return videoFrame for frame in seqNum chunk

    int chunk;
    int vidFrameInChunk;
    return findVidFrame (frame, seqNum, chunk, vidFrameInChunk) ? mChunks[chunk].getVideoFrame (vidFrameInChunk) : nullptr;
    }
  //}}}

  //{{{
  int changeChan (cHttp* http, int chan) {

    printf ("cHlsChunks::setChan %d\n", chan);
    setChan (http, chan);
    mAudBitrate = getAudMidBitrate();

    int hour = ((getDateTime()[11] - '0') * 10) + (getDateTime()[12] - '0');
    int min =  ((getDateTime()[14] - '0') * 10) + (getDateTime()[15] - '0');
    int sec =  ((getDateTime()[17] - '0') * 10) + (getDateTime()[18] - '0');
    int secsSinceMidnight = (hour * 60 * 60) + (min * 60) + sec;
    mBaseFrame = getAudFramesFromSec (secsSinceMidnight);
    printf ("cHlsRadio::changeChan- baseSeqNum:%d dateTime:%s %dh %dm %ds %d baseFrame:%d\n",
            getBaseSeqNum(), getDateTime().c_str(), hour, min, sec, secsSinceMidnight, mBaseFrame);

    invalidateChunks();
    return mBaseFrame;
    }
  //}}}
  //{{{
  void setPlayFrame (int frame) {
    mPlayFrame = frame;
    }
  //}}}
  //{{{
  void incPlayFrame (int inc) {
    setPlayFrame (mPlayFrame + inc);
    }
  //}}}
  //{{{
  void incAlignPlayFrame (int inc) {
    setPlayFrame (mPlayFrame + inc);
    }
  //}}}
  //{{{
  void setBitrate (int bitrate) {

    mAudBitrate = bitrate;
    }
  //}}}
  //{{{
  void setBitrateStrategy (bool jumped) {

    mJumped = jumped;
    if (jumped)
      // jumping around, low quality
      setBitrate (getAudLowBitrate());
    else if (getAudBitrate() == getAudLowBitrate())
      // normal play, better quality
      setBitrate (getAudMidBitrate());
    else if (getAudBitrate() == getAudMidBitrate())
      // normal play, much better quality
      setBitrate (getAudHighBitrate());
    }
  //}}}
  //{{{
  void setBitrateStrategy2 (bool jumped) {

    mJumped = false;
    if (jumped)
      setBitrate (getAudMidBitrate());
    else if (getAudBitrate() == getAudMidBitrate())
      setBitrate (getAudHighBitrate());
    }
  //}}}

  //{{{
  bool load (cHttp* http, int frame) {
  // return false if any load failed

    bool ok = true;

    mLoading = 0;
    int chunk;
    int seqNum = getSeqNumFromFrame (frame);

    if (!findSeqNumChunk (seqNum, mAudBitrate, 0, chunk)) {
      // load required chunk
      mLoading++;
      ok &= mChunks[chunk].load (http, this, seqNum, mAudBitrate);
      }

    if (!mJumped) {
      if (!findSeqNumChunk (seqNum, mAudBitrate, 1, chunk)) {
        // load chunk before
        mLoading++;
        ok &= mChunks[chunk].load (http, this, seqNum+1, mAudBitrate);
        }

      if (!findSeqNumChunk (seqNum, mAudBitrate, -1, chunk)) {
        // load chunk after
        mLoading++;
        ok &= mChunks[chunk].load (http, this, seqNum-1, mAudBitrate);
        }
      }
    mJumped = false;
    mLoading = 0;

    return ok;
    }
  //}}}

  //{{{  public vars for quick and dirty hacks
  int mTuneVol;
  int mTuneChan;
  int mPlayFrame;
  bool mPlaying;
  int mRxBytes;
  //}}}

protected:
  //{{{
  virtual void loader() {
  // loader task, handles all http gets, sleep 1s if no load suceeded

    cHttp http;
    while (true) {
      if (getChan() != mTuneChan) {
        setPlayFrame (changeChan (&http, mTuneChan) - getAudFramesFromSec(6));
        update();
        }
      if (!load (&http, mPlayFrame)) {
        printf ("sleep frame:%d\n", mPlayFrame);
        sleep (1000);
        }
      mRxBytes = http.getRxBytes();
      wait();
      }
    }
  //}}}
  virtual void signal() {}
  virtual void wait() {};
  virtual void update() {};
  virtual void sleep (int ms) {};

private:
  //{{{
  int getSeqNumFromFrame (int frame) {
  // works for -ve frame

    int r = frame - mBaseFrame;
    if (r >= 0)
      return getBaseSeqNum() + (r / getAudFramesPerChunk());
    else
      return getBaseSeqNum() - 1 - ((-r-1)/ getAudFramesPerChunk());
    }
  //}}}

  //{{{
  int getAacFrameInChunkFromFrame (int frame) {
  // works for -ve frame

    int r = (frame - mBaseFrame) % getAudFramesPerChunk();
    return r < 0 ? r + getAudFramesPerChunk() : r;
    }
  //}}}
  //{{{
  bool findAudFrame (int frame, int& seqNum, int& chunk, int& audFrameInChunk) {
  // return true, seqNum, chunk and audFrameInChunk of loadedChunk from frame
  // - return false if not found

    seqNum = getSeqNumFromFrame (frame);
    for (chunk = 0; chunk < 3; chunk++)
      if (mChunks[chunk].getSeqNum() && (seqNum == mChunks[chunk].getSeqNum())) {
        audFrameInChunk = (frame - mBaseFrame) % getAudFramesPerChunk();
        if (audFrameInChunk < 0)
          audFrameInChunk += getAudFramesPerChunk();
        if ((mChunks[chunk].getAudFramesLoaded() > 0) && (audFrameInChunk < mChunks[chunk].getAudFramesLoaded()))
          return true;
        }

    seqNum = 0;
    chunk = 0;
    audFrameInChunk = -1;
    return false;
    }
  //}}}
  //{{{
  bool findVidFrame (int frame, int& seqNum, int& chunk, int& vidFrameInChunk) {
  // return true, seqNum, chunk and vidFrameInChunk of loadedChunk from frame
  // - return false if not found

    seqNum = getSeqNumFromFrame (frame);
    for (chunk = 0; chunk < 3; chunk++)
      if (mChunks[chunk].getSeqNum() && (seqNum == mChunks[chunk].getSeqNum())) {
        vidFrameInChunk = (((frame - mBaseFrame) * getVidFramesPerChunk()) / getAudFramesPerChunk()) % getVidFramesPerChunk();
        if (vidFrameInChunk < 0)
          vidFrameInChunk += getVidFramesPerChunk();
        if ((mChunks[chunk].getVidFramesLoaded() > 0) && (vidFrameInChunk < mChunks[chunk].getVidFramesLoaded()))
          return true;
        }

    seqNum = 0;
    chunk = 0;
    vidFrameInChunk = -1;
    return false;
    }
  //}}}

  //{{{
  bool findSeqNumChunk (int seqNum, int bitrate, int offset, int& chunk) {
  // return true if match found
  // - if not, chunk = best reuse
  // - reuse same seqNum chunk if diff bitrate ?

    // look for matching chunk
    chunk = 0;
    while (chunk < 3) {
      if (seqNum + offset == mChunks[chunk].getSeqNum())
        return true;
        //return bitrate != mChunks[chunk].getBitrate();
      chunk++;
      }

    // look for stale chunk
    chunk = 0;
    while (chunk < 3) {
      if ((mChunks[chunk].getSeqNum() < seqNum-1) || (mChunks[chunk].getSeqNum() > seqNum+1))
        return false;
      chunk++;
      }

    printf ("cHlsRadio::findSeqNumChunk problem %d", seqNum);
    chunk = 0;
    return false;
    }
  //}}}
  //{{{
  std::string getChunkNumStr (int chunk) {
    return mChunks[chunk].getSeqNum() ? toString (mChunks[chunk].getSeqNum() - getBaseSeqNum()) : "*";
    }
  //}}}
  //{{{
  void invalidateChunks() {
    for (auto i = 0; i < 3; i++)
      mChunks[i].invalidate();
    }
  //}}}

  // private vars
  int mBaseFrame;
  int mAudBitrate;
  int mLoading;
  bool mJumped;
  std::string mInfoStr;
  cHlsChunk mChunks[3];
  };
