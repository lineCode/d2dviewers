// cRadioChan.h
#pragma once
#include "cParsedUrl.h"
#include "cHttp.h"

class cRadioChan {
public:
  //{{{
  cRadioChan() : mChan(0), mBaseSeqNum(0) {
    mHost[0] = 0;
    mPath[0] = 0;
    mDateTime[0] = 0;
    }
  //}}}
  ~cRadioChan() {}

  // gets
  //{{{
  int getChan() {
    return mChan;
    }
  //}}}
  //{{{
  int getSampleRate() {
    return kSampleRate;
    }
  //}}}
  //{{{
  int getSamplesPerFrame() {
    return kSamplesPerFrame;
    }
  //}}}
  //{{{
  float getFramesPerSecond() {
    return (float)kSampleRate / (float)kSamplesPerFrame;
    }
  //}}}
  //{{{
  int getFramesFromSec (int sec) {
    return int (sec * getFramesPerSecond());
    }
  //}}}
  //{{{
  int getFramesPerChunk() {
    return kFramesPerChunk [getRadioTv()];
    }
  //}}}

  //{{{
  int getLowBitrate() {
    return kLowBitrate [mChan];
    }
  //}}}
  //{{{
  int getMidBitrate() {
    return kMidBitrate [mChan];
    }
  //}}}
  //{{{
  int getHighBitrate() {
    return kHighBitrate [mChan];
    }
  //}}}

  //{{{
  int getBaseSeqNum() {
    return mBaseSeqNum;
    }
  //}}}

  //{{{
  const char* getHost() {
    return mHost;
    }
  //}}}
  //{{{
  const char* getPath (int seqNum, int bitrate) {

    if (getRadioTv()) {
      // tv
      int audioCode;
      if (bitrate == 320000)
        audioCode = 5;
      else if (bitrate == 128000)
        audioCode = 4;
      else if (bitrate == 96000)
        audioCode = 3;
      else if (bitrate == 64000)
        audioCode = 2;
      else // 24000
        audioCode = 1;
      sprintf (mPath, getTsPath(), kPool[mChan], kPathNames[mChan], kPathNames[mChan], kPathNames[mChan], audioCode, bitrate, seqNum);
      }
    else  // radio
      sprintf (mPath, getTsPath(), kPool[mChan], kPathNames[mChan], kPathNames[mChan], kPathNames[mChan], bitrate, seqNum);

    return mPath;
    }
  //}}}
  //{{{
  const char* getChanName (int chan) {
    return kChanNames [chan];
    }
  //}}}
  //{{{
  const char* getDateTime() {
    return mDateTime;
    }
  //}}}

  // set
  //{{{
  void setChan (cHttp* http, int chan) {

    mChan = chan;

    sprintf (mPath, getM3u8Path(), kPool[mChan], kPathNames[mChan], kPathNames[mChan], kPathNames[mChan], getMidBitrate());
    if (http->get (getOrigHost(), mPath) == 302) {
      strcpy (mHost, http->getRedirectedHost());
      http->get (mHost, mPath);
      }
    else
      strcpy (mHost, http->getRedirectedHost());

    printf ("%s\n", (char*)http->getContent());

    // find #EXT-X-MEDIA-SEQUENCE in .m3u8, point to seqNum string, extract seqNum from playListBuf
    auto extSeq = strstr ((char*)http->getContent(), "#EXT-X-MEDIA-SEQUENCE:") + strlen ("#EXT-X-MEDIA-SEQUENCE:");
    auto extSeqEnd = strchr (extSeq, '\n');
    *extSeqEnd = '\0';
    mBaseSeqNum = atoi (extSeq) + 3;

    auto extDateTime = strstr (extSeqEnd + 1, "#EXT-X-PROGRAM-DATE-TIME:") + strlen ("#EXT-X-PROGRAM-DATE-TIME:");
    auto extDateTimeEnd = strchr (extDateTime, '\n');
    *extDateTimeEnd = '\0';
    strcpy (mDateTime, extDateTime);
    }
  //}}}

private:
  //{{{  const
  const int kSampleRate = 48000;
  const int kSamplesPerFrame = 1024;
  const int kMaxFramesPerChunk = 375;

  const char* kOrigHost[2] =     { "as-hls-uk-live.bbcfmt.vo.llnwd.net",
                                   "vs-hls-uk-live.bbcfmt.vo.llnwd.net" };
  const char* kM3u8Path[2] =     { "pool_%d/live/%s/%s.isml/%s-audio%%3d%d.norewind.m3u8",
                                   "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d.norewind.m3u8" };
  //                                 "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d-video%3d1604000.norewind.m3u8" };
  const char* kTsPath[2] =       { "pool_%d/live/%s/%s.isml/%s-audio=%d-%d.ts",
                                   "pool_%d/live/%s/%s.isml/%s-pa%d%%3d%d-%d.ts" };
  //                                 "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d-video%%3d1604000-%d.ts" };
  // pa1 24000 pa2 64000, pa3 96000, pa4 128000, pa5 320000
  // video=437000 -video=827000 -video=1604000 -video=2812000 -video=5070000

  const int kFramesPerChunk[2] = { 300, 375 }; // 6.4s, 8s

  const bool kRadioTv    [9] = { false,   false,  false,  false,  false,  false,  false,   true,   true };
  const int kPool        [9] = {      0,      7,      7,      7,      6,      6,      6,      4,      5 };
  const int kLowBitrate  [9] = {  48000,  48000,  48000,  48000,  48000,  48000,  48000,  96000,  96000 };
  const int kMidBitrate  [9] = { 128000, 128000, 128000, 128000, 128000, 128000, 128000, 128000, 128000 };
  const int kHighBitrate [9] = { 320000, 320000, 320000, 320000, 320000, 320000, 320000,  96000, 128000 };
  const char* kChanNames [9] = { "none", "radio1", "radio2", "radio3", "radio4", "radio5", "radio6", "bbc1", "bbc2" };
  const char* kPathNames [9] = { "none", "bbc_radio_one",    "bbc_radio_two", "bbc_radio_three", "bbc_radio_fourfm",
                                         "bbc_radio_five_live", "bbc_6music",      "bbc_one_hd",       "bbc_two_hd" };
  //}}}
  //{{{
  int getRadioTv() {
    return kRadioTv[mChan];
    }
  //}}}
  //{{{
  const char* getOrigHost() {
    return kOrigHost [getRadioTv()];
    }
  //}}}
  //{{{
  const char* getM3u8Path() {
    return kM3u8Path [getRadioTv()];
    }
  //}}}
  //{{{
  const char* getTsPath() {
    return kTsPath [getRadioTv()];
    }
  //}}}

  // vars
  int mChan;
  int mBaseSeqNum;
  char mHost[80];
  char mPath[200];
  char mDateTime[80];
  };
