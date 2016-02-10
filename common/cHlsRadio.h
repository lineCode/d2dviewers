// cHlsRadio.h
#pragma once
//{{{  includes
#include "cRadioChan.h"
#include "cHlsChunk.h"
#include "iPlayer.h"
//}}}
class cHlsRadio : public cRadioChan, public iPlayer {
public:
  //{{{
  cHlsRadio() : mPlayFrame(0), mPlaying(true), mBaseFrame(0), mAudBitrate(0), mJumped(false) {}
  //}}}
  virtual ~cHlsRadio() {}

  // iPlayer interface
  //{{{
  int getAudSampleRate() {
    return cRadioChan::getAudSampleRate();
    }
  //}}}
  //{{{
  int getAudFramesFromSec (int sec) {
    return cRadioChan::getAudFramesFromSec(sec);
    }
  //}}}
  //{{{
  std::string getInfoStr (int frame) {
    return getChannelName (getChannel()) +
           (isTvChannel() ? ':' + toString (getVidBitrate()/1000) + "k" : "") +
           ':' + toString (getAudBitrate()/1000) + "k " +
           getFrameStr (frame);
    }
  //}}}
  //{{{
  std::string getFrameStr (int frame) {

    int secsSinceMidnight = int (frame / getAudFramesPerSecond());
    int secs = secsSinceMidnight % 60;
    int mins = (secsSinceMidnight / 60) % 60;
    int hours = secsSinceMidnight / (60*60);

    return toString (hours) + ':' + toString (mins) + ':' + toString (secs);
    }
  //}}}

  //{{{
  int getPlayFrame() {
    return mPlayFrame;
    }
  //}}}
  //{{{
  bool getPlaying() {
    return mPlaying;
    }
  //}}}

  //{{{
  int getSource() {
    return getChannel();
    }
  //}}}
  //{{{
  int getNumSource() {
    return getNumChannels();
    }
  //}}}
  //{{{
  std::string getSourceStr (int index) {
    return getChannelName (index);
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
  void setPlaying (bool playing) {
    mPlaying = playing;
    }
  //}}}
  //{{{
  void togglePlaying() {
    mPlaying = !mPlaying;
    }
  //}}}

  //{{{
  int changeSource (cHttp* http, int source) {

    printf ("cHlsChunks::setChan %d\n", source);
    setChannel (http, source);
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
  bool load (ID2D1DeviceContext* dc, cHttp* http, int frame) {
  // return false if any load failed

    bool ok = true;

    int chunk;
    int seqNum = getSeqNumFromFrame (frame);

    if (!findSeqNumChunk (seqNum, mAudBitrate, 0, chunk))
      ok &= mChunks[chunk].load (http, this, seqNum, mAudBitrate);

    if (!mJumped) {
      if (!findSeqNumChunk (seqNum, mAudBitrate, 1, chunk))  // load chunk after
        ok &= mChunks[chunk].load (http, this, seqNum+1, mAudBitrate);

      if (!findSeqNumChunk (seqNum, mAudBitrate, -1, chunk)) // load chunk before
        ok &= mChunks[chunk].load (http, this, seqNum-1, mAudBitrate);
      }
    mJumped = false;

    return ok;
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
  cVidFrame* getVideoFrame (int frame, int seqNum) {
  // return videoFrame for frame in seqNum chunk

    int chunk;
    int vidFrameInChunk;
    return findVidFrame (frame, seqNum, chunk, vidFrameInChunk) ? mChunks[chunk].getVideoFrame (vidFrameInChunk) : nullptr;
    }
  //}}}

  // cHlsRadio specific interface
  //{{{
  std::string getChunkInfoStr (int chunk) {
    return getChunkNumStr (chunk) + ':' + mChunks[chunk].getInfoStr();
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

private:
  //{{{
  int getAudBitrate() {
    return mAudBitrate;
    }
  //}}}
  //{{{
  void setBitrate (int bitrate) {

    mAudBitrate = bitrate;
    }
  //}}}

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
  bool findAudFrame (int frame, int& seqNum, int& chunk, int& audFrameInChunk) {
  // return true, seqNum, chunk and audFrameInChunk of loadedChunk from frame
  // - return false if not found

    seqNum = getSeqNumFromFrame (frame);
    for (chunk = 0; chunk < 3; chunk++)
      if (mChunks[chunk].getSeqNum() && (seqNum == mChunks[chunk].getSeqNum())) {
        audFrameInChunk = (frame - mBaseFrame) % getAudFramesPerChunk();
        if (audFrameInChunk < 0)
          audFrameInChunk += getAudFramesPerChunk();
        if (mChunks[chunk].getAudFramesLoaded() && (audFrameInChunk < mChunks[chunk].getAudFramesLoaded()))
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
        if (mChunks[chunk].getVidFramesLoaded() && (vidFrameInChunk < mChunks[chunk].getVidFramesLoaded()))
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
  int mPlayFrame;
  bool mPlaying;

  int mBaseFrame;
  int mAudBitrate;
  bool mJumped;
  std::string mInfoStr;
  cHlsChunk mChunks[3];
  };
