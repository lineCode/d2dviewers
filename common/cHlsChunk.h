// cHlsChunk.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
#include "cRadioChan.h"
//}}}

static int chan = 0;
static FILE* audFile = nullptr;
static FILE* vidFile = nullptr;

class cHlsChunk {
public:
  //{{{
  cHlsChunk() : mSeqNum(0), mAudBitrate(0), mFramesLoaded(0), mSamplesPerFrame(0), mAacHE(false), mDecoder(0) {
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
  int getAudBitrate() {
    return mAudBitrate;
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
  bool load (cHttp* http, cRadioChan* radioChan, int seqNum, int audBitrate) {

    mFramesLoaded = 0;
    mSeqNum = seqNum;
    mAudBitrate = audBitrate;

    // aacHE has double size frames, treat as two normal frames
    bool aacHE = mAudBitrate <= 48000;
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

    auto response = http->get (radioChan->getHost(), radioChan->getTsPath (seqNum, mAudBitrate));
    if (response == 200) {
      bool changeChan = chan != radioChan->getChan();
      chan = radioChan->getChan();

      // extract vidPes into new buffer, audPes into src buffer
      size_t vidLen = http->getContentSize();
      auto vidPesPtr = (uint8_t*)malloc (vidLen);
      auto audLen = audVidPesFromTs (http->getContent(), http->getContentEnd(), 34, 0xC0, 33, 0xE0, vidPesPtr, vidLen);
      if (audLen > 0) {
        //{{{  save audPes to .adts file, should check seqNum
        printf ("audPes:%d\n", (int)audLen);

        if (changeChan || !audFile) {
          if (audFile)
             fclose (audFile);
          std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChanName (radioChan->getChan()) +
                                 '.' + toString (getAudBitrate()) + '.' + toString (seqNum) + ".adts";
          audFile = fopen (fileName.c_str(), "wb");
          }

        fwrite (http->getContent(), 1, audLen, audFile);
        }
        //}}}
      if (vidLen > 0) {
        //{{{  save vidPes to .264 file, should check seqNum
        printf ("vidPes:%d\n", (int)vidLen);

        if (changeChan || !vidFile) {
          if (vidFile)
            fclose (vidFile);
          std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChanName (radioChan->getChan()) +
                                 '.' + toString (radioChan->getVidBitrate()) + '.' + toString (seqNum) + ".264";
          vidFile = fopen (fileName.c_str(), "wb");
          }

        fwrite (vidPesPtr, 1, vidLen, vidFile);
        }
        //}}}
      free (vidPesPtr);

      // init aacDecoder
      unsigned long sampleRate;
      uint8_t chans;
      NeAACDecInit (mDecoder, http->getContent(), 2048, &sampleRate, &chans);

      NeAACDecFrameInfo frameInfo;
      int16_t* buffer = mAudio;
      NeAACDecDecode2 (mDecoder, &frameInfo, http->getContent(), 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);

      uint8_t* powerPtr = mPower;
      auto loadPtr = http->getContent();
      while (loadPtr < http->getContent() + audLen) {
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
      mInfoStr = "ok " + toString (seqNum) + ':' + toString (audBitrate /1000) + 'k';
      return true;
      }
    else {
      mSeqNum = 0;
      mInfoStr = toString(response) + ':' + toString (seqNum) + ':' + toString (audBitrate /1000) + "k " + http->getInfoStr();
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
  size_t audVidPesFromTs (uint8_t* ptr, uint8_t* endPtr, int audPid, int audPes, int vidPid, int vidPes, uint8_t* vidPtr, size_t& vidLen) {
  // aud and vid pes from ts, return audLen

    auto audStartPtr = ptr;
    auto audPtr = ptr;
    auto vidStartPtr = vidPtr;

    while ((ptr < endPtr) && (*ptr++ == 0x47)) {
      auto payStart = *ptr & 0x40;
      auto pid = ((*ptr & 0x1F) << 8) | *(ptr+1);
      auto headerBytes = (*(ptr+2) & 0x20) ? (4 + *(ptr+3)) : 3;
      ptr += headerBytes;
      auto tsFrameBytesLeft = 187 - headerBytes;
      if (pid == audPid) {
        if (payStart && !(*ptr) && !(*(ptr+1)) && (*(ptr+2) == 1) && (*(ptr+3) == audPes)) {
          int pesHeaderBytes = 9 + *(ptr+8);
          ptr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          }
        memcpy (audPtr, ptr, tsFrameBytesLeft);
        audPtr += tsFrameBytesLeft;
        }
      else if (vidPtr && (pid == vidPid)) {
        if (payStart && !(*ptr) && !(*(ptr+1)) && (*(ptr+2) == 1) && (*(ptr+3) == vidPes)) {
          int pesHeaderBytes = 9 + *(ptr+8);
          ptr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          }
        memcpy (vidPtr, ptr, tsFrameBytesLeft);
        vidPtr += tsFrameBytesLeft;
        }
      ptr += tsFrameBytesLeft;
      }

    vidLen = vidPtr - vidStartPtr;
    return audPtr - audStartPtr;
    }
  //}}}

  // vars
  int mSeqNum;
  int mAudBitrate;
  int mFramesLoaded;
  int mSamplesPerFrame;
  std::string mInfoStr;

  bool mAacHE;
  NeAACDecHandle mDecoder;
  int16_t* mAudio;
  uint8_t* mPower;
  };
