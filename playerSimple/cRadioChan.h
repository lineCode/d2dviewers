// cRadioChan.h
#pragma once
#include "cParsedUrl.h"
#include "cHttp.h"

class cRadioChan {
public:
  cRadioChan() : mChan(0), mBaseSeqNum(0) {}
  ~cRadioChan() {}

  // gets
  //{{{
  const char* getHost() {
    return mHost;
    }
  //}}}
  //{{{
  const char* getPath (int seqNum, int bitrate) {

    sprintf (mPath, getTsPath(), kPool[mChan], kPathNames[mChan], kPathNames[mChan], kPathNames[mChan], bitrate, seqNum);
    return mPath;
    }
  //}}}
  //{{{
  const char* getChanName() {
    return kChanNames[mChan];
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
  const char* getDateTime() {
    return mDateTime;
    }
  //}}}

  // set
  //{{{
  void setChan (int chan) {

    mChan = chan;
    findM3u8SeqNum (getLowBitrate());
    }
  //}}}

private:
  //{{{  const
  const char* kOrigHost[2] =     { "as-hls-uk-live.bbcfmt.vo.llnwd.net",
                                   "vs-hls-uk-live.bbcfmt.vo.llnwd.net" };
  const char* kTsPath[2] =       { "pool_%d/live/%s/%s.isml/%s-audio=%d-%d.ts",
                                   "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d-%d.ts" };
  //                                 "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d-video%%3d1604000-%d.ts" };
  // pa1 24000 pa2 64000, pa3 96000, pa4 128000, pa5 320000
  // video=437000 -video=827000 -video=1604000 -video=2812000 -video=5070000

  const char* kM3u8Path[2] =     { "pool_%d/live/%s/%s.isml/%s-audio%%3d%d.norewind.m3u8",
                                   "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d-video%3d1604000.norewind.m3u8" };
  const int kFramesPerChunk[2] = { 300, 375 }; // 6.4s, 8s

  const bool kRadioTv    [9] = { false,   false,  false, false,   false, false,   false, true,   true };
  const int kPool        [9] = {      0,      7,      7,      7,      6,      6,      6,     5,     4 };
  const int kLowBitrate  [9] = {  48000,  48000,  48000,  48000,  48000,  48000,  48000, 96000, 96000 };
  const int kMidBitrate  [9] = { 128000, 128000, 128000, 128000, 128000, 128000, 128000, 96000, 96000 };
  const int kHighBitrate [9] = { 320000, 320000, 320000, 320000, 320000, 320000, 320000, 96000, 96000 };
  const char* kPathNames [9] = { "none", "bbc_radio_one", "bbc_radio_two", "bbc_radio_three", "bbc_radio_fourfm",
                                         "bbc_radio_five_live", "bbc_6music", "bbc_two_hd", "bbc_one_hd"};
  const char* kChanNames [9] = { "none", "bbcRadio1", "bbcRadio2", "bbcRadio3", "bbcRadio4fm",
                                         "bbcRadio5",  "bbcRadio6", "bbc2hd", "bbc1hd" };
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

  //{{{
  void findM3u8SeqNum (int bitrate) {

    cHttp m3u8;

    sprintf (mPath, getM3u8Path(), kPool[mChan], kPathNames[mChan], kPathNames[mChan], kPathNames[mChan], bitrate);
    if (m3u8.get (getOrigHost(), mPath) == 302) {
      strcpy (mHost, m3u8.getRedirectedHost());
      m3u8.get (mHost, mPath);
      }
    else
      strcpy (mHost, m3u8.getRedirectedHost());

    //printf ("%s\n", (char*)m3u8.getContent());

    // find #EXT-X-MEDIA-SEQUENCE in .m3u8, point to seqNum string, extract seqNum from playListBuf
    auto extSeq = strstr ((char*)m3u8.getContent(), "#EXT-X-MEDIA-SEQUENCE:") + strlen ("#EXT-X-MEDIA-SEQUENCE:");
    auto extSeqEnd = strchr (extSeq, '\n');
    *extSeqEnd = '\0';
    mBaseSeqNum = atoi (extSeq) + 3;

    auto extDateTime = strstr (extSeqEnd + 1, "#EXT-X-PROGRAM-DATE-TIME:") + strlen ("#EXT-X-PROGRAM-DATE-TIME:");
    auto extDateTimeEnd = strchr (extDateTime, '\n');
    *extDateTimeEnd = '\0';
    strcpy (mDateTime, extDateTime);
    }
  //}}}

  // vars
  int mChan;
  char mHost[80];
  char mPath[200];
  int mBaseSeqNum;
  char mDateTime[80];
  };
