// cHlsWindow.cpp
//{{{  includes
#include "pch.h"

#include "../common/cD2dWindow.h"

#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment (lib,"ws2_32.lib")

#include "../libfaad/include/neaacdec.h"
#pragma comment (lib,"libfaad.lib")

#include "codec_def.h"
#include "codec_app_def.h"
#include "codec_api.h"
#pragma comment (lib,"welsdec.lib")

#include "../common/cAudio.h"

#include "../common/cHttp.h"
#include "../common/cYuvFrame.h"
#include "../common/timer.h"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  }

#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
//}}}
#define kMaxChannels 10

//{{{  static vars
static int chan = 0;
static int64_t fakeVidPts = 0;

static bool avRegistered = false;
static ISVCDecoder* svcDecoder = nullptr;

static FILE* audFile = nullptr;
static FILE* vidFile = nullptr;
//}}}

//{{{
class cHlsChan {
public:
  // baseProfile:0   688000:25:640x360    281000:25:384x216   156000:25:256x144    86000:25:192x108    31000:25:192x108
  // mainProfile:1   437000:25:512x288
  // highProfile:2  5070000:50:1280x720  2812000:50:960x540  1604000:25:960x540   827000:25:704x396
  cHlsChan() : mChannel(0), mBaseSeqNum(0), mVidBitrate(156000), mVidFramesPerChunk(200), mVidProfile(0) {}
  virtual ~cHlsChan() {}

  // gets
  //{{{
  int getChannel() {
    return mChannel;
    }
  //}}}
  //{{{
  bool isTvChannel() {
    return kTv[mChannel];
    }
  //}}}
  //{{{
  int getNumChannels() {
    return kMaxChannels;
    }
  //}}}

  //{{{
  int getAudSampleRate() {
    return kAudSampleRate;
    }
  //}}}
  //{{{
  int getAudSamplesPerAacFrame() {
    return kAudSamplesPerAacFrame;
    }
  //}}}
  //{{{
  double getSecsPerAudFrame() {
    return (double)kAudSamplesPerAacFrame / (double)kAudSampleRate;
    }
  //}}}
  //{{{
  double getSecsPerVidFrame() {
    return 8.0 / mVidFramesPerChunk;
    }
  //}}}
  //{{{
  int getAudFramesPerChunk() {
    return kAudFramesPerChunk[isTvChannel()];
    }
  //}}}
  //{{{
  int getVidFramesPerChunk() {
    return mVidFramesPerChunk;
    }
  //}}}

  //{{{
  int getAudLowBitrate() {
    return kLowBitrate [mChannel];
    }
  //}}}
  //{{{
  int getAudMidBitrate() {
    return kMidBitrate [mChannel];
    }
  //}}}
  //{{{
  int getAudHighBitrate() {
    return kHighBitrate [mChannel];
    }
  //}}}
  //{{{
  int getVidBitrate() {
    return mVidBitrate;
    }
  //}}}
  //{{{
  int getVidProfile() {
    return mVidProfile;
    }
  //}}}

  //{{{
  int getBaseSeqNum() {
    return mBaseSeqNum;
    }
  //}}}

  //{{{
  std::string getHost() {
    return mHost;
    }
  //}}}
  //{{{
  std::string getTsPath (int seqNum, int bitrate) {

    return getPathRoot (bitrate) + '-' + toString (seqNum) + ".ts";
    }
  //}}}
  //{{{
  std::string getChannelName (int channel) {
    return kChannelNames [channel];
    }
  //}}}
  //{{{
  std::string getDateTime() {
    return mDateTime;
    }
  //}}}
  //{{{
  std::string getChannelInfoStr() {
    return mChanInfoStr;
    }
  //}}}

  // set
  //{{{
  void setChannel (cHttp* http, int channel) {

    if ((channel >= 0) && (channel < kMaxChannels)) {
      mChannel = channel;
      mHost = isTvChannel() ? "vs-hls-uk-live.bbcfmt.vo.llnwd.net" : "as-hls-uk-live.bbcfmt.vo.llnwd.net";

      if (http->get (mHost, getM3u8path()) == 302) {
        mHost = http->getRedirectedHost();
        http->get (mHost, getM3u8path());
        }
      else
        mHost = http->getRedirectedHost();

      //printf ("%s\n", (char*)http->getContent());

      // find #EXT-X-MEDIA-SEQUENCE in .m3u8, point to seqNum string, extract seqNum from playListBuf
      auto extSeq = strstr ((char*)http->getContent(), "#EXT-X-MEDIA-SEQUENCE:") + strlen ("#EXT-X-MEDIA-SEQUENCE:");
      auto extSeqEnd = strchr (extSeq, '\n');
      *extSeqEnd = '\0';
      mBaseSeqNum = atoi (extSeq) + 3;

      auto extDateTime = strstr (extSeqEnd + 1, "#EXT-X-PROGRAM-DATE-TIME:") + strlen ("#EXT-X-PROGRAM-DATE-TIME:");
      auto extDateTimeEnd = strchr (extDateTime, '\n');
      *extDateTimeEnd = '\0';
      mDateTime = extDateTime;

      mChanInfoStr = toString (http->getResponse()) + ' ' + http->getInfoStr() + ' ' + getM3u8path() + ' ' + mDateTime;
      }
    }
  //}}}
  //{{{
  void incVidBitrate (bool allowProfileChange, bool jumped) {

    if (jumped) {
      //mVidBitrate = 86000;
      //mVidProfile = 0;
      //mVidFramesPerChunk = 200;
      }
    else if (mVidBitrate == 86000) {
      mVidBitrate = 156000;
      mVidProfile = 0;
      mVidFramesPerChunk = 200;
      }
    else if (mVidBitrate == 156000) {
      mVidBitrate = 281000;
      mVidProfile = 0;
      mVidFramesPerChunk = 200;
      }
    else if (mVidBitrate == 281000) {
      mVidBitrate = 688000;
      mVidProfile = 0;
      mVidFramesPerChunk = 200;
      }
    else if (allowProfileChange && (mVidBitrate == 688000)) {
      mVidBitrate = 827000;
      mVidProfile = 2;
      mVidFramesPerChunk = 200;
      }
    else if (mVidBitrate == 827000) {
      mVidBitrate = 1604000;
      mVidProfile = 2;
      mVidFramesPerChunk = 200;
      }
    else if (mVidBitrate == 1604000) {
      mVidBitrate = 2812000;
      mVidProfile = 2;
      mVidFramesPerChunk = 400;
      }
    else if (mVidBitrate == 2812000) {
      mVidBitrate = 5070000;
      mVidProfile = 2;
      mVidFramesPerChunk = 400;
      }
    };
  //}}}

private:
  //{{{  const
  const int kAudSampleRate = 48000;
  const int kAudSamplesPerAacFrame = 1024;
  const int kAudFramesPerChunk[2] = { 300, 375 };  // radio:6.4s, tv:8s

  const bool kTv            [kMaxChannels] = {  false,  false,  false,  false,  false,  false,   true,   true,   true,   true };
  const int kPool           [kMaxChannels] = {      7,      7,      7,      6,      6,      6,      4,      5,      2,      3 };
  const int kLowBitrate     [kMaxChannels] = {  48000,  48000,  48000,  48000,  48000,  48000,  96000,  96000,  96000,  96000 };
  const int kMidBitrate     [kMaxChannels] = { 128000, 128000, 128000, 128000, 128000, 128000,  96000,  96000,  96000,  96000 };
  const int kHighBitrate    [kMaxChannels] = { 320000, 320000, 320000, 320000, 320000, 320000,  96000,  96000,  96000,  96000 };
  const char* kChannelNames [kMaxChannels] = { "radio1", "radio2", "radio3", "radio4", "radio5", "radio6", "bbc1", "bbc2", "bbc4", "bbcNews" };
  const char* kPathNames    [kMaxChannels] = { "bbc_radio_one", "bbc_radio_two", "bbc_radio_three", "bbc_radio_fourfm",
                                               "bbc_radio_five_live", "bbc_6music",
                                               "bbc_one_hd", "bbc_two_hd", "bbc_four_hd", "bbc_news_channel_hd" };
  //}}}
  //{{{
  std::string getPathRoot (int audBitrate) {

    std::string path = "pool_" + toString (kPool[mChannel]) + "/live/" +
                       kPathNames[mChannel] + '/' + kPathNames[mChannel] + ".isml/" + kPathNames[mChannel];
    if (isTvChannel()) {
      if (audBitrate == 96000)       // aac HE
        path += "-pa3=";
      else if (audBitrate == 128000) // aac HE
        path += "-pa4=";
      else if (audBitrate == 320000) // aac LC
        path += "-pa5=";
      else if (audBitrate == 64000)  // aac HE
        path += "-pa2=";
      else if (audBitrate == 24000)  // aac HE
        path += "-pa1=";
      else if (audBitrate == 384000) // dolby ac-3
        path += "-pa6=";
      return path + toString (audBitrate) + "-video=" + toString (getVidBitrate());
      }
   else // radio - aac HE <= 48000, LC above
     return path + "-audio=" + toString (audBitrate);
    }
  //}}}
  //{{{
  std::string getM3u8path () {
    return getPathRoot (getAudMidBitrate()) + ".norewind.m3u8";
    }
  //}}}

  // vars
  int mChannel;
  int mBaseSeqNum;
  int mVidBitrate;
  int mVidFramesPerChunk;
  int mVidProfile;

  std::string mHost;
  std::string mDateTime;
  std::string mChanInfoStr;
  };
//}}}
//{{{
class cHlsChunk {
public:
  //{{{
  cHlsChunk() : mSeqNum(0), mAudSamplesLoaded(0), mVidFramesLoaded(0),
                mAudPtr(nullptr), mAudLen(0), mVidPtr(nullptr), mVidLen(0),
                mAacHE(false), mAudDecoder(0) {

    mAudSamples = (int16_t*)malloc (375 * 1024 * 2 * 2);
    mAudPower = (uint8_t*)malloc (375 * 2);
    }
  //}}}
  //{{{
  ~cHlsChunk() {

    free (mAudPower);
    free (mAudSamples);

    if (svcDecoder) {
      svcDecoder->Uninitialize();
      WelsDestroyDecoder (svcDecoder);
      }
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
  bool load (cHttp* http, cHlsChan* hlsChan, int seqNum, int audBitrate) {

    mAudSamplesLoaded = 0;
    mVidFramesLoaded = 0;
    mSeqNum = seqNum;

    // aacHE has double size frames, treat as two normal frames
    bool aacHE = audBitrate <= 48000;

    // hhtp get chunk
    startTimer();
    auto time1 = startTimer();

    auto response = http->get (hlsChan->getHost(), hlsChan->getTsPath (seqNum, audBitrate));
    if (response == 200) {
      // allocate vidPes buffer
    auto time2 = getTimer();
      mVidPtr = hlsChan->isTvChannel() ? ((uint8_t*)malloc (hlsChan->isTvChannel() ? http->getContentSize() : 0)) : nullptr;
      pesFromTs (http->getContent(), http->getContentEnd(), 34, 0xC0, 33, 0xE0);
    auto time3 = getTimer();
      mAudPtr = http->getContent();
      decodeAudFaad (aacHE, hlsChan->getAudSamplesPerAacFrame());
    auto time4 = getTimer();
      hlsChan->getVidProfile() ? decodeVidFFmpeg() : decodeVidOpenH264();
      //saveToFile (changeChan, hlsChan);
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
  void saveToFile (cHlsChan* hlsChan, int audBitrate) {

    auto changeChan = chan != hlsChan->getChannel();
    chan = hlsChan->getChannel();

    if (mAudLen > 0) {
      // save audPes to .adts file, should check seqNum
      printf ("audPes:%d\n", (int)mAudLen);

      if (changeChan || !audFile) {
        if (audFile)
           fclose (audFile);
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + hlsChan->getChannelName (hlsChan->getChannel()) +
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
        std::string fileName = "C:\\Users\\colin\\Desktop\\test264\\" + hlsChan->getChannelName (hlsChan->getChannel()) +
                               '.' + toString (hlsChan->getVidBitrate()) + '.' + toString (mSeqNum) + ".264";
        vidFile = fopen (fileName.c_str(), "wb");
        }
      fwrite (mVidPtr, 1, mVidLen, vidFile);
      }
    }
  //}}}
  cYuvFrame mYuvFrames[400];

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
//}}}
//{{{
class cHlsLoader : public cHlsChan {
public:
  //{{{
  cHlsLoader() : mPlaying(true), mPlaySecs(0), mBaseSecs(0), mAudBitrate(0), mJumped(false) {}
  //}}}
  virtual ~cHlsLoader() {}

  // iPlayer interface
  //{{{
  int getAudSampleRate() {
    return cHlsChan::getAudSampleRate();
    }
  //}}}
  //{{{
  double getSecsPerAudFrame() {
    return cHlsChan::getSecsPerAudFrame();
    }
  //}}}
  //{{{
  double getSecsPerVidFrame() {
    return cHlsChan::getSecsPerVidFrame();
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
  void incSourceVidBitrate (bool allowProfileChange) {
    incVidBitrate (allowProfileChange, false);
    invalidateChunks();
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

    incVidBitrate (false, true);
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
//}}}

class cHlsWindow : public cD2dWindow, public cAudio {
public:
  //{{{
  cHlsWindow() {
    mSemaphore = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1
    }
  //}}}
  //{{{
  ~cHlsWindow() {
    CloseHandle (mSemaphore);
    }
  //}}}
  //{{{
  void run (wchar_t* title, int width, int height, int channel) {

    mChangeChannel = channel-1;

    mHlsLoader = new cHlsLoader();

    // init window
    initialise (title, width, height);

    // launch loaderThread
    thread ([=]() { loader(); } ).detach();

    // launch playerThread, higher priority
    auto playerThread = thread ([=]() { player(); });
    SetThreadPriority (playerThread.native_handle(), THREAD_PRIORITY_HIGHEST);
    playerThread.detach();

    // loop in windows message pump till quit
    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x10: // shift
      case 0x11: // control
      case 0x00: return false;
      case 0x1B: return true; // escape

      case 0x20: mHlsLoader->togglePlaying(); break;  // space

      case 0x21: mHlsLoader->incPlaySecs (-5*60); changed(); break; // page up
      case 0x22: mHlsLoader->incPlaySecs (+5*60); changed(); break; // page down

      //case 0x23: break; // end
      //case 0x24: break; // home

      case 0x25: mHlsLoader->incPlaySecs (-keyInc()); changed(); break;  // left arrow
      case 0x27: mHlsLoader->incPlaySecs (+keyInc()); changed(); break;  // right arrow

      case 0x26:  // up arrow
      case 0x28:  // down arrow
        mHlsLoader->setPlaying (false);
        mHlsLoader->incPlaySecs (((key == 0x26) ? -1.0 : 1.0) * keyInc() * mHlsLoader->getSecsPerVidFrame());
        changed();
        break;
      //case 0x2d: break; // insert
      //case 0x2e: break; // delete

      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36:
      case 0x37:
      case 0x38:
      case 0x39: mChangeChannel = key - '0' - 1; signal(); break;
      case 0x30: mChangeChannel = 10 - 1; signal(); break;

      default: printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    auto ratio = mControlKeyDown ? 1.5f : mShiftKeyDown ? 1.2f : 1.1f;
    if (delta > 0)
      ratio = 1.0f / ratio;
    setVolume (getVolume() * ratio);
    changed();
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

    auto showWaveform = mShowWaveform;
    mShowWaveform = inClient;

    auto showChannel = mShowChannel;
    mShowChannel = inClient && (x < 80);

    mProxChannel = -1;
    if (x < 80) {
      float yoff = 20.0f;
      for (auto source = 0; source < mHlsLoader->getNumSource(); source++) {
        if (source != mHlsLoader->getSource()) {
          if ((y >= yoff) && (y < yoff + 20.0f)) {
            mProxChannel = source;
            return;
            }
          yoff += 20.0f;
          }
        }
      }

    if ((showWaveform != mShowWaveform) || (showChannel != mShowChannel))
      changed();
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {

    mScrubbing = false;
    if (x < 80) {
      if (mProxChannel < 0) {
        mHlsLoader->incSourceVidBitrate (true);
        mDownConsumed = true;
        }
      else if (mProxChannel < mHlsLoader->getNumSource()) {
        mDownConsumed = true;
        mChangeChannel = mProxChannel;
        signal();
        changed();
        }
      }
    else if (x > int(getClientF().width-20)) {
      mDownConsumed = true;
      setVolume (y / (getClientF().height * 0.8f));
      changed();
      }
    else
      mScrubbing = true;
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    if (x > int(getClientF().width-20)) {
      setVolume (y / (getClientF().height * 0.8f));
      changed();
      }
    else {
      mHlsLoader->incPlaySecs (-xInc * mHlsLoader->getSecsPerAudFrame());
      changed();
      }
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {
    mScrubbing = false;
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (makeBitmap (mVidFrame, mBitmap, mBitmapPts))
      dc->DrawBitmap (mBitmap, D2D1::RectF(0.0f, 0.0f, getClientF().width, getClientF().height));
    else
      dc->Clear (ColorF(ColorF::Black));

    //{{{  grey mid line
    auto rMid = RectF ((getClientF().width/2)-1, 0, (getClientF().width/2)+1, getClientF().height);
    dc->FillRectangle (rMid, getGreyBrush());
    //}}}
    //{{{  yellow volume bar
    auto rVol= RectF (getClientF().width - 20,0, getClientF().width, getVolume() * 0.8f * getClientF().height);
    dc->FillRectangle (rVol, getYellowBrush());
    //}}}

    if (mShowWaveform || !mHlsLoader->isTvChannel()) {
      //{{{  blue waveform
      auto secs = mHlsLoader->getPlaySecs() - (mHlsLoader->getSecsPerAudFrame() * getClientF().width / 2.0);
      uint8_t* power = nullptr;
      auto frames = 0;
      auto rWave = RectF (0,0,1,0);
      for (; rWave.left < getClientF().width; rWave.left++, rWave.right++) {
        if (!frames)
          power = mHlsLoader->getPower (secs, frames);
        if (power) {
          rWave.top = (float)*power++;
          rWave.bottom = rWave.top + *power++;
          dc->FillRectangle (rWave, getBlueBrush());
          frames--;
          }
        secs += mHlsLoader->getSecsPerAudFrame();
        }
      }
      //}}}

    //{{{  topLine info text
    wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
    wstring wstr = converter.from_bytes (mHlsLoader->getInfoStr (mHlsLoader->getPlaySecs()));
    wstr += L" " + to_wstring(mHttpRxBytes/1000) + L"k";
    dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), RectF(0,0, getClientF().width, 20), getWhiteBrush());
    //}}}
    if (mShowChannel) {
      //{{{  show sources
      float y = 20.0f;
      for (auto source = 0; source < mHlsLoader->getNumSource(); source++) {
        if (source != mHlsLoader->getSource()) {
          wstring wstr = converter.from_bytes (mHlsLoader->getSourceStr (source));
          dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(),
                        RectF (0, y, getClientF().width, y + 20.0f),
                        (source == mProxChannel) ? getWhiteBrush() : getGreyBrush());
          y += 20.0f;
          }
        }
      }
      //}}}

    // hlsRadio debug text
    //{{{  has hlsRadio interface, show hlsRadioInfo
    if (mShowChannel) {
      if (false) {
        // show chunk debug
        wstring wstr0 = converter.from_bytes (mHlsLoader->getChunkInfoStr(0));
        wstring wstr1 = converter.from_bytes (mHlsLoader->getChunkInfoStr(0));
        wstring wstr2 = converter.from_bytes (mHlsLoader->getChunkInfoStr(0));
        dc->DrawText (wstr0.data(), (uint32_t)wstr0.size(), getTextFormat(),
                      RectF(0, getClientF().height-80, getClientF().width, getClientF().height), getWhiteBrush());
        dc->DrawText (wstr1.data(), (uint32_t)wstr1.size(), getTextFormat(),
                      RectF(0, getClientF().height-60, getClientF().width, getClientF().height), getWhiteBrush());
        dc->DrawText (wstr2.data(), (uint32_t)wstr2.size(), getTextFormat(),
                      RectF(0, getClientF().height-40, getClientF().width, getClientF().height), getWhiteBrush());
        }

      // botLine radioChan info str
      wstring wstr = converter.from_bytes (mHlsLoader->getChannelInfoStr());
      dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(),
                    RectF(0, getClientF().height-20, getClientF().width, getClientF().height), getWhiteBrush());
      }
    //}}}
    }
  //}}}

private:
  //{{{
  void loader() {
  // loader task, handles all http gets, sleep 1s if no load suceeded

    CoInitialize (NULL);

    cHttp http;
    while (true) {
      if (mHlsLoader->getSource() != mChangeChannel) {
        mHlsLoader->setPlaySecs (mHlsLoader->changeSource (&http, mChangeChannel) - 6.0);
        changed();
        }
      if (!mHlsLoader->load (getDeviceContext(), &http, mHlsLoader->getPlaySecs())) {
        printf ("sleep frame:%3.2f\n", mHlsLoader->getPlaySecs());
        Sleep (1000);
        }
      mHttpRxBytes = http.getRxBytes();
      wait();
      }

    CoUninitialize();
    }
  //}}}
  //{{{
  void player() {

    CoInitialize (NULL);
    audOpen (mHlsLoader->getAudSampleRate(), 16, 2);

    auto lastSeqNum = 0;
    while (true) {
      int seqNum;
      auto audSamples = mHlsLoader->getAudSamples (mHlsLoader->getPlaySecs(), seqNum);
      audPlay (mHlsLoader->getPlaying() ? audSamples : nullptr, 4096, 1.0f);
      mVidFrame = mHlsLoader->getVidFrame (mHlsLoader->getPlaySecs(), seqNum);

      if (audSamples && mHlsLoader->getPlaying() && !mScrubbing) {
        mHlsLoader->incPlaySecs (mHlsLoader->getSecsPerAudFrame());
        changed();
        }

      if (!seqNum || (seqNum != lastSeqNum)) {
        mHlsLoader->setBitrateStrategy (seqNum != lastSeqNum+1);
        lastSeqNum = seqNum;
        signal();
        }
      }

    audClose();
    CoUninitialize();
    }
  //}}}
  //{{{
  void wait() {
    WaitForSingleObject (mSemaphore, 20 * 1000);
    }
  //}}}
  //{{{
  void signal() {
    ReleaseSemaphore (mSemaphore, 1, NULL);
    }
  //}}}
  //{{{  vars
  cHlsLoader* mHlsLoader = nullptr;

  bool mScrubbing = false;
  int mProxChannel = 0;
  int mChangeChannel = 0;

  int mHttpRxBytes = 0;

  bool mShowWaveform = false;
  bool mShowChannel = false;

  HANDLE mSemaphore;

  int64_t mBitmapPts = 0;
  cYuvFrame* mVidFrame = nullptr;
  ID2D1Bitmap* mBitmap = nullptr;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  WSADATA wsaData;
  if (WSAStartup (MAKEWORD(2,2), &wsaData)) {
    //{{{  error exit
    printf ("WSAStartup failed\n");
    exit (0);
    }
    //}}}

#ifndef _DEBUG
  FreeConsole();
#endif

  cHlsWindow hlsWindow;
  hlsWindow.run (L"hlsRadioWindow", 480, 272, argc >= 2 ? _wtoi (argv[1]) : 4);
  }
//}}}
