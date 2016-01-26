// cHlsChunks.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
#include "cRadioChan.h"
#include "cHlsChunk.h"
//}}}

class cHlsChunks {
public:
  #define SILENCE_SIZE 4096
  #define NUM_CHUNKS 3
  //{{{
  cHlsChunks() {
    for (auto i = 0; i < SILENCE_SIZE; i++)
      mSilence[i] = 0;
    }
  //}}}

  //{{{
  int getBaseSeqNum() {
    return mRadioChan.getBaseSeqNum();
    }
  //}}}

  //{{{
  void setChan (int chan, int bitrate) {
    mRadioChan.setChan (chan, bitrate);
    invalidateChunks();
    }
  //}}}
  //{{{
  bool load (int playFrame) {

    int seqNum = cHlsChunk::getFrameSeqNum (playFrame);

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
  int16_t* play (int playFrame, bool& playing, int& seqNum) {

    int chunk;
    int frameInChunk;
    playing &= findFrame (playFrame, seqNum, chunk, frameInChunk);

    return playing ? mChunks[chunk].getAudioSamples (frameInChunk) : mSilence;
    }
  //}}}
  //{{{
  bool power (int frame, uint8_t** powerPtr, int& frames) {

    int seqNum;
    int chunk;
    int frameInChunk;
    if (findFrame (frame, seqNum, chunk, frameInChunk)) {
      frames = cHlsChunk::getNumFrames() - frameInChunk;
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
  bool findFrame (int frame, int& seqNum, int& chunk, int& frameInChunk) {

    for (auto i = 0; i < NUM_CHUNKS; i++) {
      if (mChunks[i].contains (frame, seqNum, frameInChunk)) {
        chunk = i;
        return true;
        }
      }

    seqNum = 0;
    chunk = 0;
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
  cHlsChunk mChunks [NUM_CHUNKS];
  int16_t mSilence [SILENCE_SIZE];
  };
