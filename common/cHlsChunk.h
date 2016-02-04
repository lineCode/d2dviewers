// cHlsChunk.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
#include "cRadioChan.h"
//}}}

class cHlsChunk {
public:
  //{{{
  cHlsChunk() : mSeqNum(0), mBitrate(0), mFramesLoaded(0), mSamplesPerFrame(0), mAacHE(false), mDecoder(0) {
    mAudio = (int16_t*)pvPortMalloc (375 * 1024 * 2 * 2);
    mPower = (uint8_t*)malloc (375 * 2);
    }
  //}}}
  //{{{
  ~cHlsChunk() {
    if (mPower)
      free (mPower);
    if (mAudio)
      vPortFree (mAudio);
    }
  //}}}

  // gets
  //{{{
  int getSeqNum() {
    return mSeqNum;
    }
  //}}}
  //{{{
  int getBitrate() {
    return mBitrate;
    }
  //}}}
  //{{{
  int getFramesLoaded() {
    return mFramesLoaded;
    }
  //}}}
  //{{{
  std::string getInfoStr() {
    return mInfoStr;
    }
  //}}}
  //{{{
  uint8_t* getAudioPower (int frameInChunk, int& frames) {

    frames = getFramesLoaded() - frameInChunk;
    return mPower ? mPower + (frameInChunk * 2) : nullptr;
    }
  //}}}
  //{{{
  int16_t* getAudioSamples (int frameInChunk) {
    return mAudio ? (mAudio + (frameInChunk * mSamplesPerFrame * 2)) : nullptr;
    }
  //}}}

  //{{{
  bool load (cHttp* http, cRadioChan* radioChan, int seqNum, int bitrate) {

    mFramesLoaded = 0;
    mSeqNum = seqNum;
    mBitrate = bitrate;

    // aacHE has double size frames, treat as two normal frames
    bool aacHE = mBitrate <= 48000;
    int framesPerAacFrame = aacHE ? 2 : 1;
    mSamplesPerFrame = radioChan->getSamplesPerFrame();
    int samplesPerAacFrame = mSamplesPerFrame * framesPerAacFrame;

    if ((mDecoder == 0) || (aacHE != mAacHE)) {
      if (aacHE != mAacHE)
        NeAACDecClose (mDecoder);
      mDecoder = NeAACDecOpen();
      NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mDecoder);
      config->outputFormat = FAAD_FMT_16BIT;
      NeAACDecSetConfiguration (mDecoder, config);
      mAacHE = aacHE;
      }

    auto response = http->get (radioChan->getHost(), radioChan->getTsPath (seqNum, mBitrate));
    if (response == 200) {
      // extract video to file
      std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChanName (radioChan->getChan()) +
                             '.' + toString (radioChan->getVidBitrate()) + '.' + toString (seqNum) + ".264";
      extractVideo (fileName, http->getContent(), http->getContentEnd());

      // pack audio down into same buffer
      auto loadEnd = packTsBuffer (http->getContent(), http->getContentEnd());

      auto loadPtr = http->getContent();
      // init decoder
      unsigned long sampleRate;
      uint8_t chans;
      NeAACDecInit (mDecoder, loadPtr, 2048, &sampleRate, &chans);

      NeAACDecFrameInfo frameInfo;
      int16_t* buffer = mAudio;
      NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);

      uint8_t* powerPtr = mPower;
      while (loadPtr < loadEnd) {
        NeAACDecDecode2 (mDecoder, &frameInfo, loadPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);
        loadPtr += frameInfo.bytesconsumed;
        for (int i = 0; i < framesPerAacFrame; i++) {
          //{{{  calc left, right power
          int valueL = 0;
          int valueR = 0;
          for (int j = 0; j < mSamplesPerFrame; j++) {
            short sample = (*buffer++) >> 4;
            valueL += sample * sample;
            sample = (*buffer++) >> 4;
            valueR += sample * sample;
            }

          uint8_t leftPix = (uint8_t)sqrt(valueL / (mSamplesPerFrame * 32.0f));
          *powerPtr++ = (272/2) - leftPix;
          *powerPtr++ = leftPix + (uint8_t)sqrt(valueR / (mSamplesPerFrame * 32.0f));
          mFramesLoaded++;
          }
          //}}}
        }

      http->freeContent();
      mInfoStr = "ok " + toString (seqNum) + ':' + toString (bitrate/1000) + 'k';
      return true;
      }
    else {
      mSeqNum = 0;
      mInfoStr = toString(response) + ':' + toString (seqNum) + ':' + toString (bitrate/1000) + "k " + http->getInfoStr();
      return false;
      }
    }
  //}}}
  //{{{
  void invalidate() {

    mSeqNum = 0;
    mFramesLoaded = 0;
    }
  //}}}

private:
  //{{{
  void extractVideo (std::string fileName, uint8_t* ptr, uint8_t* endPtr) {
  // extract video pes

    int pesVidIndex = 0;
    int lastVidPidContinuity = -1;
    unsigned char* pesVid = (unsigned char*)malloc (2000000);

    while (ptr < endPtr) {
      if (*ptr == 0x47) {
        // tsFrame syncCode found, extract pes from tsFrames
        bool payStart  =  (*(ptr+1) & 0x40) != 0;
        int pid        = ((*(ptr+1) & 0x1F) << 8) | *(ptr+2);
        int continuity =   *(ptr+3) & 0x0F;
        bool payload   =  (*(ptr+3) & 0x10) != 0;
        bool adapt     =  (*(ptr+3) & 0x20) != 0;
        int tsFrameIndex = adapt ? (5 + *(ptr+4)) : 4;

        if (pid == 33) {
          if (payload && ((lastVidPidContinuity == -1) || (continuity == ((lastVidPidContinuity+1) & 0x0F)))) {
            // look for video pes startCode
            if (payStart && (*(ptr+tsFrameIndex) == 0) && (*(ptr+tsFrameIndex+1) == 0) &&
                            (*(ptr+tsFrameIndex+2) == 1) && (*(ptr+tsFrameIndex+3) == 0xE0)) {
              // PES start code found
              int pesHeadLen = *(ptr+tsFrameIndex+8);
              tsFrameIndex += 9 + pesHeadLen;
              }
            while (tsFrameIndex < 188)
              pesVid[pesVidIndex++] = *(ptr+tsFrameIndex++);
            }
          else
            printf ("- ts vidContinuity break last:%d this:%d\n", lastVidPidContinuity, continuity);
          lastVidPidContinuity = continuity;
          }
        ptr += 188;
        }
      else
        ptr++;
      }

    // save video pes to file
    printf ("vidPes:%d\n", pesVidIndex);
    if (pesVidIndex > 0) {
      FILE* vidFile = fopen (fileName.c_str(), "wb");
      fwrite (pesVid, 1, pesVidIndex, vidFile);
      fclose (vidFile);
      }

    free (pesVid);
    }
  //}}}
  //{{{
  uint8_t* packTsBuffer (uint8_t* ptr, uint8_t* endPtr) {
  // pack transportStream down to aac frames in same buffer

    auto toPtr = ptr;
    while ((ptr < endPtr) && (*ptr++ == 0x47)) {
      auto payStart = *ptr & 0x40;
      auto pid = ((*ptr & 0x1F) << 8) | *(ptr+1);
      auto headerBytes = (*(ptr+2) & 0x20) ? (4 + *(ptr+3)) : 3;
      ptr += headerBytes;
      auto tsFrameBytesLeft = 187 - headerBytes;
      if (pid == 34) {
        if (payStart && !(*ptr) && !(*(ptr+1)) && (*(ptr+2) == 1) && (*(ptr+3) == 0xC0)) {
          int pesHeaderBytes = 9 + *(ptr+8);
          ptr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          }
        memcpy (toPtr, ptr, tsFrameBytesLeft);
        toPtr += tsFrameBytesLeft;
        }
      ptr += tsFrameBytesLeft;
      }

    return toPtr;
    }
  //}}}

  // vars
  int mSeqNum;
  int mBitrate;
  int mFramesLoaded;
  int mSamplesPerFrame;
  std::string mInfoStr;

  bool mAacHE;
  NeAACDecHandle mDecoder;
  int16_t* mAudio;
  uint8_t* mPower;
  };
