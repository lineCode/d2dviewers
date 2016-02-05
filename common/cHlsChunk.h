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
static ISVCDecoder* pDecoder = nullptr;
static ComPtr<IWICImagingFactory> wicImagingFactory;

class cHlsChunk {
public:
  //{{{
  cHlsChunk() : mSeqNum(0), mAudBitrate(0), mFramesLoaded(0), mVidFramesLoaded(0), mSamplesPerFrame(0), mAacHE(false), mDecoder(0) {
    mAudio = (int16_t*)pvPortMalloc (375 * 1024 * 2 * 2);
    mPower = (uint8_t*)malloc (375 * 2);

  #ifdef WIN32
    for (auto i = 0; i < 200; i++)
      vidFrames[i] = nullptr;
  #endif
    }
  //}}}
  //{{{
  ~cHlsChunk() {
    if (mPower)
      free (mPower);
    if (mAudio)
      vPortFree (mAudio);

  #ifdef WIN32
    if (pDecoder) {
      pDecoder->Uninitialize();
      WelsDestroyDecoder (pDecoder);
      }
  #endif
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
  int getVidFramesLoaded() {
    return mVidFramesLoaded;
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
#ifdef WIN32
  //{{{
  IWICBitmap* getVideoFrame (int videoFrameInChunk) {
    return vidFrames [videoFrameInChunk];
    }
  //}}}
#endif
  //{{{
  bool load (cHttp* http, cRadioChan* radioChan, int seqNum, int audBitrate) {

    mFramesLoaded = 0;
    mVidFramesLoaded = 0;
    mSeqNum = seqNum;
    mAudBitrate = audBitrate;

    // aacHE has double size frames, treat as two normal frames
    bool aacHE = mAudBitrate <= 48000;
    int framesPerAacFrame = aacHE ? 2 : 1;
    mSamplesPerFrame = radioChan->getSamplesPerFrame();

    if ((mDecoder == 0) || (aacHE != mAacHE)) {
      //{{{  init audDecoder
      if (aacHE != mAacHE)
        NeAACDecClose (mDecoder);
      mDecoder = NeAACDecOpen();
      NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mDecoder);
      config->outputFormat = FAAD_FMT_16BIT;
      NeAACDecSetConfiguration (mDecoder, config);
      mAacHE = aacHE;
      }
      //}}}

    auto response = http->get (radioChan->getHost(), radioChan->getTsPath (seqNum, mAudBitrate));
    if (response == 200) {
      bool changeChan = chan != radioChan->getChan();
      chan = radioChan->getChan();

      size_t vidLen = radioChan->getRadioTv() ? http->getContentSize() : 0; 
      uint8_t* vidPesPtr = radioChan->getRadioTv() ? ((uint8_t*)malloc (vidLen)) : nullptr;
      auto audLen = audVidPesFromTs (http->getContent(), http->getContentEnd(), 34, 0xC0, 33, 0xE0, vidPesPtr, vidLen);
      //saveToFile (changeChan, radioChan, seqNum, http->getContent(), audLen, vidPesPtr, vidLen);

      processAudio (http->getContent(), audLen, framesPerAacFrame);
      http->freeContent();

      processVideo (vidPesPtr, vidLen);
      if (vidPesPtr)
        free (vidPesPtr);

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
  size_t audVidPesFromTs (uint8_t* ptr, uint8_t* endPtr, int audPid, int audPes,
                          int vidPid, int vidPes, uint8_t* vidPtr, size_t& vidLen) {
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
  //{{{
  void processAudio (uint8_t* audPtr, size_t audLen, int framesPerAacFrame) {

    // init aacDecoder
    unsigned long sampleRate;
    uint8_t chans;
    NeAACDecInit (mDecoder, audPtr, 2048, &sampleRate, &chans);

    NeAACDecFrameInfo frameInfo;
    int16_t* buffer = mAudio;
    int samplesPerAacFrame = mSamplesPerFrame * framesPerAacFrame;
    NeAACDecDecode2 (mDecoder, &frameInfo, audPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);

    uint8_t* powerPtr = mPower;
    auto ptr = audPtr;
    while (ptr < audPtr + audLen) {
      NeAACDecDecode2 (mDecoder, &frameInfo, ptr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);
      ptr += frameInfo.bytesconsumed;
      for (int i = 0; i < framesPerAacFrame; i++) {
        // calc left, right power
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
      }
    }
  //}}}
  //{{{
  uint8_t limit (double v) {

    if (v <= 0.0)
      return 0;

    if (v >= 255.0)
      return 255;

    return (uint8_t)v;
    }
  //}}}
  //{{{
  void processVideo (uint8_t* ptr, size_t vidLen) {

    if (vidLen > 0) {

  #ifdef WIN32
      if (!pDecoder) {
        //{{{  init static decoder
        WelsCreateDecoder (&pDecoder);

        int iLevelSetting = (int)WELS_LOG_WARNING;
        pDecoder->SetOption (DECODER_OPTION_TRACE_LEVEL, &iLevelSetting);

        // init decoder params
        SDecodingParam sDecParam = {0};
        sDecParam.sVideoProperty.size = sizeof (sDecParam.sVideoProperty);
        sDecParam.uiTargetDqLayer = (uint8_t) - 1;
        sDecParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
        sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
        pDecoder->Initialize (&sDecParam);

        // set conceal option
        int32_t iErrorConMethod = (int32_t) ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
        pDecoder->SetOption (DECODER_OPTION_ERROR_CON_IDC, &iErrorConMethod);

        // create vidFrame wicBitmap 24bit BGR
        CoCreateInstance (CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&wicImagingFactory));
        }
        //}}}

      // append slice startCode
      uint8_t uiStartCode[4] = {0, 0, 0, 1};
      memcpy (ptr + vidLen, &uiStartCode[0], 4);

      int frameIndex = 0;
      unsigned long long uiTimeStamp = 0;
      int32_t iBufPos = 0;
      while (true) {
        // process slice
        int32_t iEndOfStreamFlag = 0;
        if (iBufPos >= vidLen) {
          //{{{  end of stream
          iEndOfStreamFlag = true;
          pDecoder->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
          break;
          }
          //}}}

        // find sliceSize by looking for next slice startCode
        int32_t iSliceSize;
        for (iSliceSize = 1; iSliceSize < vidLen; iSliceSize++)
          if ((!ptr[iBufPos+iSliceSize] && !ptr[iBufPos+iSliceSize+1])  &&
              ((!ptr[iBufPos+iSliceSize+2] && ptr[iBufPos+iSliceSize+3] == 1) || (ptr[iBufPos+iSliceSize+2] == 1)))
            break;

        uint8_t* pData[3]; // yuv ptrs
        SBufferInfo sDstBufInfo;
        memset (&sDstBufInfo, 0, sizeof (SBufferInfo));
        sDstBufInfo.uiInBsTimeStamp = uiTimeStamp++;

        pDecoder->DecodeFrameNoDelay (ptr + iBufPos, iSliceSize, pData, &sDstBufInfo);

        if (sDstBufInfo.iBufferStatus == 1) {
          //{{{  make frame
          int width = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
          int pitch = width;
          int height = sDstBufInfo.UsrData.sSystemBuffer.iHeight;

          if (!vidFrames[frameIndex])
            wicImagingFactory->CreateBitmap (pitch, height, GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand, &vidFrames[frameIndex]);

          WICRect wicRect = { 0, 0, pitch, height };
          IWICBitmapLock* wicBitmapLock = NULL;
          vidFrames[frameIndex]->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);

          // get vidFrame wicBitmap buffer
          UINT bufferLen = 0;
          BYTE* buffer = NULL;
          wicBitmapLock->GetDataPointer (&bufferLen, &buffer);

          // yuv
          for (auto y = 0; y < height; y++) {
            BYTE* yptr = pData[0] + (y*sDstBufInfo.UsrData.sSystemBuffer.iStride[0]);
            BYTE* uptr = pData[1] + ((y/2)*sDstBufInfo.UsrData.sSystemBuffer.iStride[1]);
            BYTE* vptr = pData[2] + ((y/2)*sDstBufInfo.UsrData.sSystemBuffer.iStride[1]);

            for (auto x = 0; x < width/2; x++) {
              int y1 = *yptr++;
              int y2 = *yptr++;
              int u = (*uptr++) - 128;
              int v = (*vptr++) - 128;

              *buffer++ = limit (y1 + (1.8556 * u));
              *buffer++ = limit (y1 - (0.1873 * u) - (0.4681 * v));
              *buffer++ = limit (y1 + (1.5748 * v));

              *buffer++ = limit (y2 + (1.8556 * u));
              *buffer++ = limit (y2 - (0.1873 * u) - (0.4681 * v));
              *buffer++ = limit (y2 + (1.5748 * v));
              }
            }

          // release vidFrame wicBitmap buffer
          wicBitmapLock->Release();

          mVidFramesLoaded++;
          frameIndex++;
          }
          //}}}

        iBufPos += iSliceSize;
        }
  #endif

      }
    }
  //}}}
  //{{{
  void saveToFile (bool changeChan, cRadioChan* radioChan, int seqNum, uint8_t* audPtr, size_t audLen, uint8_t* vidPtr, size_t& vidLen) {

    if (audLen > 0) {
      // save audPes to .adts file, should check seqNum
      printf ("audPes:%d\n", (int)audLen);

      if (changeChan || !audFile) {
        if (audFile)
           fclose (audFile);
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChanName (radioChan->getChan()) +
                               '.' + toString (getAudBitrate()) + '.' + toString (seqNum) + ".adts";
        audFile = fopen (fileName.c_str(), "wb");
        }
      fwrite (audPtr, 1, audLen, audFile);
      }

    if (vidLen > 0) {
      // save vidPes to .264 file, should check seqNum
      printf ("vidPes:%d\n", (int)vidLen);

      if (changeChan || !vidFile) {
        if (vidFile)
          fclose (vidFile);
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChanName (radioChan->getChan()) +
                               '.' + toString (radioChan->getVidBitrate()) + '.' + toString (seqNum) + ".264";
        vidFile = fopen (fileName.c_str(), "wb");
        }
      fwrite (vidPtr, 1, vidLen, vidFile);
      }
    }
  //}}}

  // vars
  int mSeqNum;
  int mAudBitrate;
  int mFramesLoaded;
  int mVidFramesLoaded;
  int mSamplesPerFrame;
  std::string mInfoStr;

  bool mAacHE;
  NeAACDecHandle mDecoder;
  int16_t* mAudio;
  uint8_t* mPower;

#ifdef WIN32
  IWICBitmap* vidFrames[200];
#endif
  };
