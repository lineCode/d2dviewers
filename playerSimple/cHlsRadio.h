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
  #define SILENCE_SIZE 4096
  #define NUM_CHUNKS 3
  //{{{
  cHlsRadio() : mBaseSeqNum(0), mBaseFrame(0) {
    for (auto i = 0; i < SILENCE_SIZE; i++)
      mSilence[i] = 0;
    }
  //}}}

  //{{{
  int setChan (int chan, int bitrate) {

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
  bool load (int frame) {

    int seqNum = getSeqNumFromFrame (frame);
    printf ("cHlsChunks::load frame::%d seqNum::%d\n", frame, seqNum);

    bool ok = false;
    int chunk;
    if (!findSeqNumChunk (seqNum, 0, chunk))
      ok &= mChunks[chunk].load (&mRadioChan, seqNum);

    for (auto i = 1; i <= NUM_CHUNKS/2; i++) {
      if (!findSeqNumChunk (seqNum, i, chunk))
        ok &= mChunks[chunk].load (&mRadioChan, seqNum+i);
      if (!findSeqNumChunk (seqNum, -i, chunk))
        ok &= mChunks[chunk].load (&mRadioChan, seqNum-i);
      }

    return ok;
    }
  //}}}
  //{{{
  int16_t* play (int frame, bool& playing, int& seqNum) {

    int chunk;
    int frameInChunk;
    playing &= findFrame (frame, seqNum, chunk, frameInChunk);

    return playing ? mChunks[chunk].getAudioSamples (frameInChunk) : mSilence;
    }
  //}}}
  //{{{
  bool power (int frame, uint8_t** powerPtr, int& frames) {

    int seqNum;
    int chunk;
    int frameInChunk;
    if (findFrame (frame, seqNum, chunk, frameInChunk)) {
      frames = cHlsChunk::getFramesPerChunk() - frameInChunk;
      mChunks[chunk].getAudioPower (frameInChunk, powerPtr);
      return true;
      }
    else {
      frames = 0;
      return false;
      }
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
  bool findSeqNumChunk (int seqNum, int offset, int& chunk) {

    // look for matching chunk
    chunk = 0;
    while (chunk < NUM_CHUNKS) {
      if (seqNum + offset == mChunks[chunk].getSeqNum())
        return true;
      chunk++;
      }

    // look for stale chunk
    chunk = 0;
    while (chunk < NUM_CHUNKS) {
      if ((mChunks[chunk].getSeqNum() < seqNum-(NUM_CHUNKS/2)) || (mChunks[chunk].getSeqNum() > seqNum+(NUM_CHUNKS/2)))
        return false;
      chunk++;
      }

    printf ("findSeqNumChunk cockup %d", seqNum);
    for (auto i = 0; i < NUM_CHUNKS; i++)
      printf (" %d", mChunks[i].getSeqNum());
    printf ("\n");

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
  cHlsChunk mChunks [NUM_CHUNKS];
  int16_t mSilence [SILENCE_SIZE];
  };
