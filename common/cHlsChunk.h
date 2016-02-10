// cHlsChunk.h
#pragma once
//{{{  includes
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

#ifdef WIN32
  static bool avRegistered = false;
  static int videoId = 0;
  static ISVCDecoder* svcDecoder = nullptr;

  static FILE* audFile = nullptr;
  static FILE* vidFile = nullptr;
#endif
//}}}

class cHlsChunk {
public:
  //{{{
  cHlsChunk() : mSeqNum(0), mAudBitrate(0), mAudFramesLoaded(0), mVidFramesLoaded(0),
                mAudPtr(nullptr), mAudLen(0), mVidPtr(nullptr), mVidLen(0),
                mAudSamplesPerAacFrame(0), mFramesPerAacFrame(0), mAacHE(false), mDecoder(0) {

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
  int getAudBitrate() {
    return mAudBitrate;
    }
  //}}}
  //{{{
  int getAudFramesLoaded() {
    return mAudFramesLoaded;
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

    frames = getAudFramesLoaded() - frameInChunk;
    return mPower ? mPower + (frameInChunk * 2) : nullptr;
    }
  //}}}
  //{{{
  int16_t* getAudioSamples (int frameInChunk) {
    return mAudio ? (mAudio + (frameInChunk * mAudSamplesPerAacFrame * 2)) : nullptr;
    }
  //}}}
#ifdef WIN32
  //{{{
  cVidFrame* getVideoFrame (int videoFrameInChunk) {
    return &vidFrames [videoFrameInChunk];
    }
  //}}}
#endif
  //{{{
  bool load (cHttp* http, cRadioChan* radioChan, int seqNum, int audBitrate) {

    mAudFramesLoaded = 0;
    mVidFramesLoaded = 0;
    mSeqNum = seqNum;
    mAudBitrate = audBitrate;

    // aacHE has double size frames, treat as two normal frames
    bool aacHE = mAudBitrate <= 48000;
    mFramesPerAacFrame = aacHE ? 2 : 1;
    mAudSamplesPerAacFrame = radioChan->getAudSamplesPerAacFrame();

    // hhtp get chunk
    startTimer();
    auto time1 = startTimer();
    auto response = http->get (radioChan->getHost(), radioChan->getTsPath (seqNum, mAudBitrate));
    if (response == 200) {
      // allocate vidPes buffer
    auto time2 = getTimer();
      mVidPtr = radioChan->isTvChannel() ? ((uint8_t*)malloc (radioChan->isTvChannel() ? http->getContentSize() : 0)) : nullptr;
      pesFromTs (http->getContent(), http->getContentEnd(), 34, 0xC0, 33, 0xE0);
    auto time3 = getTimer();
      mAudPtr = http->getContent();
      decodeAudFaad (aacHE);
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
    mAudFramesLoaded = 0;
    }
  //}}}

private:
  //{{{
  void pesFromTs (uint8_t* ptr, uint8_t* endPtr, int audPid, int audPes, int vidPid, int vidPes) {
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
  void decodeAudFaad (bool aacHE) {

    if (!mDecoder || (aacHE != mAacHE)) {
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

    // init aacDecoder
    unsigned long sampleRate;
    uint8_t chans;
    NeAACDecInit (mDecoder, mAudPtr, 2048, &sampleRate, &chans);

    NeAACDecFrameInfo frameInfo;
    int16_t* buffer = mAudio;
    int samplesPerAacFrame = mAudSamplesPerAacFrame * mFramesPerAacFrame;
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
        for (int j = 0; j < mAudSamplesPerAacFrame; j++) {
          short sample = (*buffer++) >> 4;
          valueL += sample * sample;
          sample = (*buffer++) >> 4;
          valueR += sample * sample;
          }

        uint8_t leftPix = (uint8_t)sqrt(valueL / (mAudSamplesPerAacFrame * 32.0f));
        *powerPtr++ = (272/2) - leftPix;
        *powerPtr++ = leftPix + (uint8_t)sqrt(valueR / (mAudSamplesPerAacFrame * 32.0f));

        mAudFramesLoaded++;
        }
      }
    }
  //}}}
#ifdef WIN32
  //{{{
  void saveYuvFrame (uint8_t** yuv, int* strides, int width, int height) {

    vidFrames[mVidFramesLoaded].mYStride = strides[0];
    vidFrames[mVidFramesLoaded].mUVStride = strides[1];
    vidFrames[mVidFramesLoaded].mWidth = width;
    vidFrames[mVidFramesLoaded].mHeight = height;
    vidFrames[mVidFramesLoaded].mId = videoId++;

    if (vidFrames[mVidFramesLoaded].mYbuf)
      free (vidFrames[mVidFramesLoaded].mYbuf);
    vidFrames[mVidFramesLoaded].mYbuf = (uint8_t*)malloc (vidFrames[mVidFramesLoaded].mHeight * vidFrames[mVidFramesLoaded].mYStride);
    memcpy (vidFrames[mVidFramesLoaded].mYbuf, yuv[0], vidFrames[mVidFramesLoaded].mHeight * vidFrames[mVidFramesLoaded].mYStride);

    if (vidFrames[mVidFramesLoaded].mUbuf)
      free (vidFrames[mVidFramesLoaded].mUbuf);
    vidFrames[mVidFramesLoaded].mUbuf = (uint8_t*)malloc ((vidFrames[mVidFramesLoaded].mHeight/2) * vidFrames[mVidFramesLoaded].mUVStride);
    memcpy (vidFrames[mVidFramesLoaded].mUbuf, yuv[1], (vidFrames[mVidFramesLoaded].mHeight/2) * vidFrames[mVidFramesLoaded].mUVStride);

    if (vidFrames[mVidFramesLoaded].mVbuf)
      free (vidFrames[mVidFramesLoaded].mVbuf);
    vidFrames[mVidFramesLoaded].mVbuf = (uint8_t*)malloc ((vidFrames[mVidFramesLoaded].mHeight/2) * vidFrames[mVidFramesLoaded].mUVStride);
    memcpy (vidFrames[mVidFramesLoaded].mVbuf, yuv[2], (vidFrames[mVidFramesLoaded].mHeight/2) * vidFrames[mVidFramesLoaded].mUVStride);

    mVidFramesLoaded++;
    }
  //}}}
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
          saveYuvFrame (yuv, sDstBufInfo.UsrData.sSystemBuffer.iStride,
                        sDstBufInfo.UsrData.sSystemBuffer.iWidth, sDstBufInfo.UsrData.sSystemBuffer.iHeight);

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

      AVCodec* vidCodec = avcodec_find_decoder (AV_CODEC_ID_H264);
      AVCodecParserContext* vidParser = av_parser_init (AV_CODEC_ID_H264);
      AVCodecContext* vidCodecContext = avcodec_alloc_context3 (vidCodec);
      avcodec_open2 (vidCodecContext, vidCodec, NULL);
      AVFrame* vidFrame = av_frame_alloc();

      AVPacket vidPacket;
      av_init_packet (&vidPacket);
      vidPacket.data = mVidPtr;
      vidPacket.size = 0;

      int bytesUsed = 0;
      int vidLen = (int)mVidLen;
      while (bytesUsed || vidLen) {
        uint8_t* data = NULL;
        av_parser_parse2 (vidParser, vidCodecContext, &data, &vidPacket.size, vidPacket.data, vidLen, 0, 0, AV_NOPTS_VALUE);

        int gotPicture = 0;
        bytesUsed = avcodec_decode_video2 (vidCodecContext, vidFrame, &gotPicture, &vidPacket);
        if (gotPicture)
          saveYuvFrame (vidFrame->data, vidFrame->linesize, vidCodecContext->width, vidCodecContext->height);

        vidPacket.data += bytesUsed;
        vidLen -= bytesUsed;
        }

      av_free (vidFrame);
      avcodec_close (vidCodecContext);
      }
    }
  //}}}
  //{{{
  void saveToFile (cRadioChan* radioChan) {

    bool changeChan = chan != radioChan->getChannel();
    chan = radioChan->getChannel();

    if (mAudLen > 0) {
      // save audPes to .adts file, should check seqNum
      printf ("audPes:%d\n", (int)mAudLen);

      if (changeChan || !audFile) {
        if (audFile)
           fclose (audFile);
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChannelName (radioChan->getChannel()) +
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
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + radioChan->getChannelName (radioChan->getChannel()) +
                               '.' + toString (radioChan->getVidBitrate()) + '.' + toString (mSeqNum) + ".264";
        vidFile = fopen (fileName.c_str(), "wb");
        }
      fwrite (mVidPtr, 1, mVidLen, vidFile);
      }
    }
  //}}}
#endif

  //{{{  private vars
  int mSeqNum;
  int mAudBitrate;
  int mAudFramesLoaded;
  int mVidFramesLoaded;
  int mAudSamplesPerAacFrame;
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
  cVidFrame vidFrames[400];
  #endif
  //}}}
  };
