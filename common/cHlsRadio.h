// cHlsRadio.h
#pragma once
//{{{  includes
class cYuvFrame;
#include "cRadioChan.h"
#include "cHlsChunk.h"
#include "iPlayer.h"
//}}}
class cHlsRadio : public cRadioChan, public iPlayer {
public:
  //{{{
  cHlsRadio() : mPlaying(true), mPlaySecs(0), mBaseSecs(0), mAudBitrate(0), mJumped(false) {}
  //}}}
  virtual ~cHlsRadio() {}

  // iPlayer interface
  //{{{
  int getAudSampleRate() {
    return cRadioChan::getAudSampleRate();
    }
  //}}}
  //{{{
  double getSecsPerAudFrame() {
    return cRadioChan::getSecsPerAudFrame();
    }
  //}}}
  //{{{
  double getSecsPerVidFrame() {
    return cRadioChan::getSecsPerVidFrame();
    }
  //}}}
  //{{{
  std::string getInfoStr (double secs) {
    return getChannelName (getChannel()) +
           (isTvChannel() ? ':' + toString (getVidBitrate()/1000) + "k" : "") +
           ':' + toString (getAudBitrate()/1000) + "k " +
           toString (int(secs) / (60 * 60)) + ':' + toString ((int(secs) / 60) % 60) + ':' + toString (int(secs) % 60);
    }
  //}}}

  //{{{
  bool getPlaying() {
    return mPlaying;
    }
  //}}}
  //{{{
  double getPlaySecs() {
    return mPlaySecs;
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
  double changeSource (cHttp* http, int source) {

    printf ("cHlsChunks::setChan %d\n", source);
    setChannel (http, source);
    mAudBitrate = getAudMidBitrate();

    int hour = ((getDateTime()[11] - '0') * 10) + (getDateTime()[12] - '0');
    int min =  ((getDateTime()[14] - '0') * 10) + (getDateTime()[15] - '0');
    int sec =  ((getDateTime()[17] - '0') * 10) + (getDateTime()[18] - '0');
    int secs = (hour * 60 * 60) + (min * 60) + sec;
    mBaseSecs = secs;
    printf ("cHlsRadio::changeChan- baseSeqNum:%d dateTime:%s %dh %dm %ds %d baseFrame:%3.2f\n",
            getBaseSeqNum(), getDateTime().c_str(), hour, min, sec, secs, mBaseSecs);

    invalidateChunks();
    return mBaseSecs;
    }
  //}}}

  //{{{
  void setPlaySecs (double secs) {
    mPlaySecs = secs;
    }
  //}}}
  //{{{
  void incPlaySecs (double secs) {
    setPlaySecs (mPlaySecs + secs);
    }
  //}}}
  //{{{
  void incAlignPlaySecs (double secs) {
    setPlaySecs (mPlaySecs + secs);
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
  bool load (ID2D1DeviceContext* dc, cHttp* http, double secs) {
  // return false if any load failed

    bool ok = true;

    int chunk;
    int seqNum = getSeqNumFromSecs (secs);

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
  uint8_t* getPower (double secs, int& frames) {
  // return pointer to frame power org,len uint8_t pairs
  // frames = number of valid frames

    int seqNum;
    int chunk;
    int frameInChunk;
    int sampleInChunk;

    if (findAudFrame (secs, seqNum, chunk, frameInChunk, sampleInChunk)) {
      frames = (mChunks[chunk].getAudSamplesLoaded() / getAudSamplesPerAacFrame()) - frameInChunk;
      return mChunks[chunk].getAudPower() + (frameInChunk * 2);
      }
    else {
      frames = 0;
      return nullptr;
      }
    }
  //}}}
  //{{{
  int16_t* getAudSamples (double secs, int& seqNum) {
  // return audio buffer for frame

    int chunk;
    int frameInChunk;
    int sampleInChunk;

    if (findAudFrame (secs, seqNum, chunk, frameInChunk, sampleInChunk))
      return mChunks[chunk].getAudSamples() + sampleInChunk*2;
    else
      return nullptr;
    }
  //}}}
  //{{{
  cYuvFrame* getVidFrame (double secs, int seqNum) {
  // return videoFrame for frame in seqNum chunk

    int chunk;
    int vidFrameInChunk;
    return findVidFrame (secs, seqNum, chunk, vidFrameInChunk) ? mChunks[chunk].getVidFrame (vidFrameInChunk) : nullptr;
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
  int getSeqNumFromSecs (double secs) {
  // works for -ve frame

    int audFrame = int ((secs - mBaseSecs) / getSecsPerAudFrame());

    if (audFrame >= 0)
      return getBaseSeqNum() + (audFrame / getAudFramesPerChunk());
    else
      return getBaseSeqNum() - 1 - ((-audFrame-1)/ getAudFramesPerChunk());
    }
  //}}}
  //{{{
  bool findAudFrame (double secs, int& seqNum, int& chunk, int& audFrameInChunk, int& audSampleInChunk) {
  // return true, seqNum, chunk and audFrameInChunk of loadedChunk from frame
  // - return false if not found

    seqNum = getSeqNumFromSecs (secs);

    for (chunk = 0; chunk < 3; chunk++)
      if (mChunks[chunk].getSeqNum() && (seqNum == mChunks[chunk].getSeqNum())) {
        int audFrame = int((secs - mBaseSecs) / getSecsPerAudFrame());
        audFrameInChunk = audFrame % getAudFramesPerChunk();
        if (audFrameInChunk < 0)
          audFrameInChunk += getAudFramesPerChunk();
        audSampleInChunk = audFrameInChunk * getAudSamplesPerAacFrame();
        if (mChunks[chunk].getAudSamplesLoaded() && (audSampleInChunk < mChunks[chunk].getAudSamplesLoaded()))
          return true;
        }

    seqNum = 0;
    chunk = 0;
    audFrameInChunk = 0;
    audSampleInChunk = 0;
    return false;
    }
  //}}}
  //{{{
  bool findVidFrame (double secs, int& seqNum, int& chunk, int& vidFrameInChunk) {
  // return true, seqNum, chunk and vidFrameInChunk of loadedChunk from frame
  // - return false if not found

    seqNum = getSeqNumFromSecs (secs);

    for (chunk = 0; chunk < 3; chunk++)
      if (mChunks[chunk].getSeqNum() && (seqNum == mChunks[chunk].getSeqNum())) {
        int vidFrame = int ((secs - mBaseSecs) / getSecsPerVidFrame());
        vidFrameInChunk = vidFrame % getVidFramesPerChunk();
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
  bool mPlaying;
  double mPlaySecs;
  double mBaseSecs;
  int mAudBitrate;
  bool mJumped;
  std::string mInfoStr;
  cHlsChunk mChunks[3];
  };
