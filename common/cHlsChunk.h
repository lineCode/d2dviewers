// cHlsChunk.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
#include "cRadioChan.h"

#ifdef WIN32
  #ifdef __cplusplus
    extern "C" {
  #endif
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
  #ifdef __cplusplus
    }
  #endif

  #pragma comment(lib,"avutil.lib")
  #pragma comment(lib,"avcodec.lib")
  #pragma comment(lib,"avformat.lib")
  #pragma comment(lib,"swscale.lib")
#endif
//}}}
//{{{  static vars
static int chan = 0;

#ifdef WIN32
  static FILE* audFile = nullptr;
  static FILE* vidFile = nullptr;
  static ISVCDecoder* svcDecoder = nullptr;
  static ComPtr<IWICImagingFactory> wicImagingFactory;

  static AVCodec* vidCodec = NULL;
  static AVCodecContext* vidCodecContext = NULL;
  static AVCodecParserContext* vidParser = NULL;
  static AVFrame* vidFrame = NULL;
  static struct SwsContext* swsContext = NULL;
#endif
//}}}

class cHlsChunk {
public:
  //{{{
  cHlsChunk() : mSeqNum(0), mAudBitrate(0), mFramesLoaded(0), mVidFramesLoaded(0),
                mAudPtr(nullptr), mAudLen(0), mVidPtr(nullptr), mVidLen(0),
                mSamplesPerFrame(0), mFramesPerAacFrame(0), mAacHE(false), mDecoder(0) {

    mAudio = (int16_t*)pvPortMalloc (375 * 1024 * 2 * 2);
    mPower = (uint8_t*)malloc (375 * 2);

  #ifdef WIN32
    for (auto i = 0; i < 400; i++)
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
    if (svcDecoder) {
      svcDecoder->Uninitialize();
      WelsDestroyDecoder (svcDecoder);
      }

  av_free (vidFrame);
  avcodec_close (vidCodecContext);
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
    mFramesPerAacFrame = aacHE ? 2 : 1;
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
      mVidPtr = radioChan->getRadioTv() ? ((uint8_t*)malloc (radioChan->getRadioTv() ? http->getContentSize() : 0)) : nullptr;
      audVidPesFromTs (http->getContent(), http->getContentEnd(), 34, 0xC0, 33, 0xE0);
      mAudPtr = http->getContent();
      processAudioFaad();
      radioChan->getVidProfile() ? processVideoFFmpeg() : processVideoOpenH264();
      //saveToFile (changeChan, radioChan);
      if (mVidPtr) free (mVidPtr);
      http->freeContent();

      mInfoStr = "ok " + toString (seqNum) + ':' + toString (audBitrate /1000) + 'k';
      return true;
      }
    else {
      mSeqNum = 0;
      mInfoStr = toString (response) + ':' + toString (seqNum) + ':' + toString (audBitrate /1000) + "k " + http->getInfoStr();
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
  void audVidPesFromTs (uint8_t* ptr, uint8_t* endPtr, int audPid, int audPes, int vidPid, int vidPes) {
  // aud and vid pes from ts

    auto audStartPtr = ptr;
    auto audPtr = ptr;
    auto vidStartPtr = mVidPtr;
    auto vidPtr = mVidPtr;

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

    mVidLen = vidPtr - vidStartPtr;
    mAudLen = audPtr - audStartPtr;
    }
  //}}}
  //{{{
  void processAudioFaad() {

    // init aacDecoder
    unsigned long sampleRate;
    uint8_t chans;
    NeAACDecInit (mDecoder, mAudPtr, 2048, &sampleRate, &chans);

    NeAACDecFrameInfo frameInfo;
    int16_t* buffer = mAudio;
    int samplesPerAacFrame = mSamplesPerFrame * mFramesPerAacFrame;
    NeAACDecDecode2 (mDecoder, &frameInfo, mAudPtr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);

    uint8_t* powerPtr = mPower;
    auto ptr = mAudPtr;
    while (ptr < mAudPtr + mAudLen) {
      NeAACDecDecode2 (mDecoder, &frameInfo, ptr, 2048, (void**)(&buffer), samplesPerAacFrame * 2 * 2);
      ptr += frameInfo.bytesconsumed;
      for (int i = 0; i < mFramesPerAacFrame; i++) {
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
#ifdef WIN32
  //{{{
  void saveToFile (cRadioChan* radioChan) {

    bool changeChan = chan != radioChan->getChan();
    chan = radioChan->getChan();

    if (mAudLen > 0) {
      // save audPes to .adts file, should check seqNum
      printf ("audPes:%d\n", (int)mAudLen);

      if (changeChan || !audFile) {
        if (audFile)
           fclose (audFile);
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChanName (radioChan->getChan()) +
                               '.' + toString (getAudBitrate()) + '.' + toString (mSeqNum) + ".adts";
        audFile = fopen (fileName.c_str(), "wb");
        }
      fwrite (mAudPtr, 1, mAudLen, audFile);
      }

    if (mVidLen > 0) {
      // save vidPes to .264 file, should check seqNum
      printf ("vidPes:%d\n", (int)mVidLen);

      if (changeChan || !vidFile) {
        if (vidFile)
          fclose (vidFile);
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChanName (radioChan->getChan()) +
                               '.' + toString (radioChan->getVidBitrate()) + '.' + toString (mSeqNum) + ".264";
        vidFile = fopen (fileName.c_str(), "wb");
        }
      fwrite (mVidPtr, 1, mVidLen, vidFile);
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
  void processVideoOpenH264() {

    if (mVidLen) {
      if (!svcDecoder) {
        //{{{  init static decoder
        WelsCreateDecoder (&svcDecoder);

        int iLevelSetting = (int)WELS_LOG_WARNING;
        svcDecoder->SetOption (DECODER_OPTION_TRACE_LEVEL, &iLevelSetting);

        // init decoder params
        SDecodingParam sDecParam = {0};
        sDecParam.sVideoProperty.size = sizeof (sDecParam.sVideoProperty);
        sDecParam.uiTargetDqLayer = (uint8_t) - 1;
        sDecParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
        sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
        svcDecoder->Initialize (&sDecParam);

        // set conceal option
        int32_t iErrorConMethod = (int32_t)ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;
        svcDecoder->SetOption (DECODER_OPTION_ERROR_CON_IDC, &iErrorConMethod);

        // create vidFrame wicBitmap 24bit BGR
        CoCreateInstance (CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&wicImagingFactory));
        }
        //}}}

      unsigned long long uiTimeStamp = 0;
      int32_t iBufPos = 0;
      while (true) {
        // calc next sliceSize, look for next sliceStartCode, not sure about end of buf test, relies on 4 trailing bytes of anything
        int32_t iSliceSize;
        for (iSliceSize = 1; iSliceSize < mVidLen - iBufPos; iSliceSize++)
          if ((!mVidPtr[iBufPos+iSliceSize] && !mVidPtr[iBufPos+iSliceSize+1])  &&
              ((!mVidPtr[iBufPos+iSliceSize+2] && mVidPtr[iBufPos+iSliceSize+3] == 1) || (mVidPtr[iBufPos+iSliceSize+2] == 1)))
            break;

        // process slice
        uint8_t* pData[3]; // yuv ptrs
        SBufferInfo sDstBufInfo;
        memset (&sDstBufInfo, 0, sizeof (SBufferInfo));
        sDstBufInfo.uiInBsTimeStamp = uiTimeStamp++;

        svcDecoder->DecodeFrameNoDelay (mVidPtr + iBufPos, iSliceSize, pData, &sDstBufInfo);

        if (sDstBufInfo.iBufferStatus == 1) {
          //{{{  have new frame
          int width = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
          int height = sDstBufInfo.UsrData.sSystemBuffer.iHeight;

          // could test for size change
          if (!vidFrames[mVidFramesLoaded])
            wicImagingFactory->CreateBitmap (width, height, GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand, &vidFrames[mVidFramesLoaded]);

          WICRect wicRect = { 0, 0, width, height };
          IWICBitmapLock* wicBitmapLock = NULL;
          vidFrames[mVidFramesLoaded]->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);

          // get vidFrame wicBitmap buffer
          UINT bufferLen = 0;
          BYTE* buffer = NULL;
          wicBitmapLock->GetDataPointer (&bufferLen, &buffer);

          // yuv:420 -> BGR:24bit
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
          }
          //}}}

        iBufPos += iSliceSize;
        if (iBufPos >= mVidLen) {
          // end of stream
          int32_t iEndOfStreamFlag = true;
          svcDecoder->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
          break;
          }
        }
      }
    }
  //}}}
  //{{{
  void processVideoFFmpeg() {

    if (mVidLen) {
      if (!vidCodec) {
        //{{{  init static decoder
        // init vidCodecContext, vidCodec, vidParser, vidFrame
        av_register_all();

        vidCodec = avcodec_find_decoder (AV_CODEC_ID_H264);;
        vidCodecContext = avcodec_alloc_context3 (vidCodec);;
        avcodec_open2 (vidCodecContext, vidCodec, NULL);

        vidParser = av_parser_init (AV_CODEC_ID_H264);;

        vidFrame = av_frame_alloc();;
        swsContext = NULL;

        // create vidFrame wicBitmap 24bit BGR
        CoCreateInstance (CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&wicImagingFactory));
        }
        //}}}

      AVPacket vidPacket;
      av_init_packet (&vidPacket);
      vidPacket.data = mVidPtr;
      vidPacket.size = 0;

      int vidLen = (int)mVidLen;
      while (vidLen > 0) {
        uint8_t* data = NULL;
        av_parser_parse2 (vidParser, vidCodecContext, &data, &vidPacket.size, vidPacket.data, vidLen, 0, 0, AV_NOPTS_VALUE);

        int gotPicture = 0;
        int len = avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &vidPacket);
        vidPacket.data += len;
        vidLen -= len;

        if (gotPicture != 0) {
          if (!swsContext)
            swsContext = sws_getContext (vidCodecContext->width, vidCodecContext->height, vidCodecContext->pix_fmt,
                                         vidCodecContext->width, vidCodecContext->height, AV_PIX_FMT_BGR24,
                                         SWS_BICUBIC, NULL, NULL, NULL);
          if (!vidFrames[mVidFramesLoaded])
            wicImagingFactory->CreateBitmap (vidCodecContext->width, vidCodecContext->height,
                                             GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand, &vidFrames[mVidFramesLoaded]);
          //{{{  lock wicBitmap
          WICRect wicRect = { 0, 0, vidCodecContext->width, vidCodecContext->height };
          IWICBitmapLock* wicBitmapLock = NULL;
          vidFrames[mVidFramesLoaded]->Lock (&wicRect, WICBitmapLockWrite, &wicBitmapLock);
          //}}}
          //{{{  get wicBitmap buffer
          UINT bufferLen = 0;
          BYTE* buffer = NULL;
          wicBitmapLock->GetDataPointer (&bufferLen, &buffer);
          //}}}
          int linesize = vidCodecContext->width * 3;
          sws_scale (swsContext, vidFrame->data, vidFrame->linesize, 0, vidCodecContext->height, &buffer, &linesize);
          wicBitmapLock->Release();

          mVidFramesLoaded++;
         }
        }
      }
    }
  //}}}
#endif

  //{{{  private vars
  int mSeqNum;
  int mAudBitrate;
  int mFramesLoaded;
  int mVidFramesLoaded;
  int mSamplesPerFrame;
  int mFramesPerAacFrame;
  std::string mInfoStr;

  uint8_t* mAudPtr;
  size_t mAudLen;
  uint8_t* mVidPtr;
  size_t mVidLen;

  bool mAacHE;
  NeAACDecHandle mDecoder;
  int16_t* mAudio;
  uint8_t* mPower;

  #ifdef WIN32
    IWICBitmap* vidFrames[400];
  #endif
  //}}}
  };
