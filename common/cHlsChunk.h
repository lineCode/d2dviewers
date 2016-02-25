// cHlsChunk.h
#pragma once
//{{{  includes
#include "cYuvFrame.h"
#include "cParsedUrl.h"
#include "cHttp.h"
#include "iPlayer.h"
#include "cRadioChan.h"

#ifdef WIN32
  #include "../common/timer.h"

  #ifdef __cplusplus
    extern "C" {
  #endif
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
  #ifdef __cplusplus
    }
  #endif

  #pragma comment(lib,"avutil.lib")
  #pragma comment(lib,"avcodec.lib")
  #pragma comment(lib,"avformat.lib")
#endif
//}}}
//{{{  static vars
static int chan = 0;
static int64_t fakeVidPts = 0;

#ifdef WIN32
  static bool avRegistered = false;
  static ISVCDecoder* svcDecoder = nullptr;

  static FILE* audFile = nullptr;
  static FILE* vidFile = nullptr;
#endif
//}}}

class cHlsChunk {
public:
  //{{{
  cHlsChunk() : mSeqNum(0), mAudSamplesLoaded(0), mVidFramesLoaded(0),
                mAudPtr(nullptr), mAudLen(0), mVidPtr(nullptr), mVidLen(0),
                mAacHE(false), mAudDecoder(0) {

    mAudSamples = (int16_t*)pvPortMalloc (375 * 1024 * 2 * 2);
    mAudPower = (uint8_t*)malloc (375 * 2);
    }
  //}}}
  //{{{
  ~cHlsChunk() {

    if (mAudPower)
      free (mAudPower);
    if (mAudSamples)
      vPortFree (mAudSamples);

  #ifdef WIN32
    if (svcDecoder) {
      svcDecoder->Uninitialize();
      WelsDestroyDecoder (svcDecoder);
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
  int getAudSamplesLoaded() {
    return mAudSamplesLoaded;
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
  uint8_t* getAudPower() {
    return mAudPower;
    }
  //}}}
  //{{{
  int16_t* getAudSamples() {
    return mAudSamples;
    }
  //}}}
  //{{{
  cYuvFrame* getVidFrame (int videoFrameInChunk) {
    return &mYuvFrames [videoFrameInChunk];
    }
  //}}}

  //{{{
  bool load (cHttp* http, cRadioChan* radioChan, int seqNum, int audBitrate) {

    mAudSamplesLoaded = 0;
    mVidFramesLoaded = 0;
    mSeqNum = seqNum;

    // aacHE has double size frames, treat as two normal frames
    bool aacHE = audBitrate <= 48000;

    // hhtp get chunk
    startTimer();
    auto time1 = startTimer();

    auto response = http->get (radioChan->getHost(), radioChan->getTsPath (seqNum, audBitrate));
    if (response == 200) {
      // allocate vidPes buffer
    auto time2 = getTimer();
      mVidPtr = radioChan->isTvChannel() ? ((uint8_t*)malloc (radioChan->isTvChannel() ? http->getContentSize() : 0)) : nullptr;
      pesFromTs (http->getContent(), http->getContentEnd(), 34, 0xC0, 33, 0xE0);
    auto time3 = getTimer();
      mAudPtr = http->getContent();
      decodeAudFaad (aacHE, radioChan->getAudSamplesPerAacFrame());
    auto time4 = getTimer();
      radioChan->getVidProfile() ? decodeVidFFmpeg() : decodeVidOpenH264();
      //saveToFile (changeChan, radioChan);
    auto time5 = getTimer();
      if (mVidPtr) { free (mVidPtr); mVidPtr = nullptr; }
      http->freeContent();

      printf ("get:%7.6f pes:%7.6f aac:%7.6f vid:%7.6f tot:%7.6f - aud:%d vid:%d get:%d\n",
              time2-time1, time3-time2, time4-time3, time5-time4, time5-time1,
              (int)mAudLen, (int)mVidLen, http->getContentSize());
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
    mAudSamplesLoaded = 0;

    for (auto i = 0; i < 400; i++)
      mYuvFrames[i].freeResources();
    }
  //}}}

private:
  //{{{
  int64_t parseTimeStamp (uint8_t* tsPtr) {
    return (((*tsPtr) & 0x0E) << 29) | (*(tsPtr+1) << 22) | ((*(tsPtr+2) & 0xFE) << 14) | (*(tsPtr+3) << 7)  | (*(tsPtr+4) >> 1);
    }
  //}}}
  //{{{
  void parseTimeStamps (uint8_t* tsPtr, int64_t& pts, int64_t& dts) {

    pts = ((*(tsPtr+8) >= 5) && (*(tsPtr+7) & 0x80)) ? parseTimeStamp (tsPtr+9) : 0;
    dts = ((*(tsPtr+8) >= 10) && (*(tsPtr+7) & 0x40)) ? parseTimeStamp (tsPtr+14) : 0;
    }
  //}}}
  //{{{
  void pesFromTs (uint8_t* tsPtr, uint8_t* endPtr, int audPid, int audPes, int vidPid, int vidPes) {
  // aud and vid pes from ts

    auto audStartPtr = tsPtr;
    auto audPtr = tsPtr;
    auto vidStartPtr = mVidPtr;
    auto vidPtr = mVidPtr;

    while ((tsPtr < endPtr) && (*tsPtr++ == 0x47)) {
      auto payStart = *tsPtr & 0x40;
      auto pid = ((*tsPtr & 0x1F) << 8) | *(tsPtr+1);
      auto adaption = (*(tsPtr+2) & 0x20) != 0;
      auto hasPcr = adaption && (*(tsPtr+4) & 0x10);
      int64_t pcr = hasPcr ? ((*(tsPtr+5)<<25) | (*(tsPtr+6)<<17)  | (*(tsPtr+7)<<9) | (*(tsPtr+8)<<1) | (*(tsPtr+9)>>7)) : 0;
      auto headerBytes = adaption ? (4 + *(tsPtr+3)) : 3;
      tsPtr += headerBytes;
      auto tsFrameBytesLeft = 187 - headerBytes;

      if (pid == audPid) {
        if (payStart && !(*tsPtr) && !(*(tsPtr+1)) && (*(tsPtr+2) == 1) && (*(tsPtr+3) == audPes)) {
          int64_t pts;
          int64_t dts;
          parseTimeStamps (tsPtr, pts, dts);

          int pesHeaderBytes = 9 + *(tsPtr+8);
          tsPtr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          }
        memcpy (audPtr, tsPtr, tsFrameBytesLeft);
        audPtr += tsFrameBytesLeft;
        }

      else if (vidPtr && (pid == vidPid)) {
        if (payStart && !(*tsPtr) && !(*(tsPtr+1)) && (*(tsPtr+2) == 1) && (*(tsPtr+3) == vidPes)) {
          int64_t pts;
          int64_t dts;
          parseTimeStamps (tsPtr, pts, dts);

          int pesHeaderBytes = 9 + *(tsPtr+8);
          tsPtr += pesHeaderBytes;
          tsFrameBytesLeft -= pesHeaderBytes;
          }
        memcpy (vidPtr, tsPtr, tsFrameBytesLeft);
        vidPtr += tsFrameBytesLeft;
        }

      tsPtr += tsFrameBytesLeft;
      }

    mVidLen = vidPtr - vidStartPtr;
    mAudLen = audPtr - audStartPtr;
    }
  //}}}
  //{{{
  void decodeAudFaad (bool aacHE, int samplesPerAacFrame) {

    if (!mAudDecoder || (aacHE != mAacHE)) {
      //{{{  init audDecoder
      if (aacHE != mAacHE)
        NeAACDecClose (mAudDecoder);

      mAudDecoder = NeAACDecOpen();
      NeAACDecConfiguration* config = NeAACDecGetCurrentConfiguration (mAudDecoder);
      config->outputFormat = FAAD_FMT_16BIT;
      NeAACDecSetConfiguration (mAudDecoder, config);

      mAacHE = aacHE;
      }
      //}}}

    // init aacDecoder
    unsigned long sampleRate;
    uint8_t chans;
    NeAACDecInit (mAudDecoder, mAudPtr, 2048, &sampleRate, &chans);

    NeAACDecFrameInfo frameInfo;
    auto buffer = mAudSamples;

    auto actualSamplesPerAacFrame = aacHE ? samplesPerAacFrame*2 :samplesPerAacFrame;
    NeAACDecDecode2 (mAudDecoder, &frameInfo, mAudPtr, 2048, (void**)(&buffer), actualSamplesPerAacFrame * 2 * 2);

    auto powerPtr = mAudPower;
    auto ptr = mAudPtr;
    while (ptr < mAudPtr + mAudLen) {
      NeAACDecDecode2 (mAudDecoder, &frameInfo, ptr, 2048, (void**)(&buffer), actualSamplesPerAacFrame * 2 * 2);
      ptr += frameInfo.bytesconsumed;
      for (auto i = 0; i < (aacHE ? 2 : 1); i++) {
        // calc left, right power
        int valueL = 0;
        int valueR = 0;
        for (auto j = 0; j < samplesPerAacFrame; j++) {
          auto sample = (*buffer++) >> 4;
          valueL += sample * sample;
          sample = (*buffer++) >> 4;
          valueR += sample * sample;
          }

        auto leftPix = (uint8_t)sqrt(valueL / (samplesPerAacFrame * 32.0f));
        *powerPtr++ = (272/2) - leftPix;
        *powerPtr++ = leftPix + (uint8_t)sqrt(valueR / (samplesPerAacFrame * 32.0f));

        mAudSamplesLoaded += samplesPerAacFrame;
        }
      }
    }
  //}}}

#ifdef WIN32
  //{{{
  void decodeVidOpenH264() {

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
        uint8_t* yuv[3];
        SBufferInfo sDstBufInfo;
        memset (&sDstBufInfo, 0, sizeof (SBufferInfo));
        sDstBufInfo.uiInBsTimeStamp = uiTimeStamp++;

        svcDecoder->DecodeFrameNoDelay (mVidPtr + iBufPos, iSliceSize, yuv, &sDstBufInfo);
        if (sDstBufInfo.iBufferStatus)
          mYuvFrames[mVidFramesLoaded++].set (fakeVidPts++, yuv, sDstBufInfo.UsrData.sSystemBuffer.iStride,
                                              sDstBufInfo.UsrData.sSystemBuffer.iWidth,
                                              sDstBufInfo.UsrData.sSystemBuffer.iHeight, 0,0);

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
  void decodeVidFFmpeg() {

    if (mVidLen) {
      if (!avRegistered) {
        av_register_all();
        avRegistered = true;
        }

      auto vidParser = av_parser_init (AV_CODEC_ID_H264);
      auto vidCodec = avcodec_find_decoder (AV_CODEC_ID_H264);
      auto vidCodecContext = avcodec_alloc_context3 (vidCodec);
      avcodec_open2 (vidCodecContext, vidCodec, NULL);

      AVPacket vidPacket;
      av_init_packet (&vidPacket);
      vidPacket.data = mVidPtr;
      vidPacket.size = 0;

      auto packetUsed = 0;
      auto vidLen = (int)mVidLen;
      auto vidFrame = av_frame_alloc();
      while (packetUsed || vidLen) {
        uint8_t* data = NULL;
        av_parser_parse2 (vidParser, vidCodecContext, &data, &vidPacket.size, vidPacket.data, vidLen, 0, 0, AV_NOPTS_VALUE);

        auto got = 0;
        packetUsed = avcodec_decode_video2 (vidCodecContext, vidFrame, &got, &vidPacket);
        if (got)
          mYuvFrames[mVidFramesLoaded++].set (fakeVidPts++, vidFrame->data, vidFrame->linesize,
                                              vidCodecContext->width, vidCodecContext->height, 0, 0);

        vidPacket.data += packetUsed;
        vidLen -= packetUsed;
        }
      av_frame_free (&vidFrame);

      av_parser_close (vidParser);
      avcodec_close (vidCodecContext);
      }
    }
  //}}}
  //{{{
  void saveToFile (cRadioChan* radioChan, int audBitrate) {

    auto changeChan = chan != radioChan->getChannel();
    chan = radioChan->getChannel();

    if (mAudLen > 0) {
      // save audPes to .adts file, should check seqNum
      printf ("audPes:%d\n", (int)mAudLen);

      if (changeChan || !audFile) {
        if (audFile)
           fclose (audFile);
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChannelName (radioChan->getChannel()) +
                               '.' + toString (audBitrate) + '.' + toString (mSeqNum) + ".adts";
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
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChannelName (radioChan->getChannel()) +
                               '.' + toString (radioChan->getVidBitrate()) + '.' + toString (mSeqNum) + ".264";
        vidFile = fopen (fileName.c_str(), "wb");
        }
      fwrite (mVidPtr, 1, mVidLen, vidFile);
      }
    }
  //}}}
  cYuvFrame mYuvFrames[400];
#endif

  //{{{  private vars
  int mSeqNum;
  int mAudSamplesLoaded;
  int mVidFramesLoaded;
  std::string mInfoStr;

  // temps for load
  uint8_t* mAudPtr;
  size_t mAudLen;
  uint8_t* mVidPtr;
  size_t mVidLen;

  bool mAacHE;
  NeAACDecHandle mAudDecoder;

  int16_t* mAudSamples;
  uint8_t* mAudPower;
  //}}}
  };
