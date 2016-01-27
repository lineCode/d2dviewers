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
  const char* kOrigHost[2] = { "as-hls-uk-live.bbcfmt.vo.llnwd.net",
                               "vs-hls-uk-live.bbcfmt.vo.llnwd.net" };
  const char* kTsPath[2] =   { "pool_%d/live/%s/%s.isml/%s-audio=%d-%d.ts",
  //                             "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d-video%%3d1604000-%d.ts" };
                               "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d-%d.ts" };
  const char* kM3u8Path[2] = { "pool_%d/live/%s/%s.isml/%s-audio%%3d%d.norewind.m3u8",
                               "pool_%d/live/%s/%s.isml/%s-pa3%%3d%d-video%3d1604000.norewind.m3u8" };
  const int kFramesPerChunk[2] = { 300, 375 };

  const int kRadioTv [8] =     { 0, 0, 0, 0, 0, 0, 0, 1 };
  const int kPool [8] =        { 0, 0, 0, 7, 6, 0, 6, 5 };
  const int kLowBitrate [8] =  {  48000, 48000,  48000,  48000,  48000,  48000,  48000,  96000 };
  const int kMidBitrate [8] =  { 128000, 128000, 128000, 128000, 128000, 128000, 128000, 96000 };
  const int kHighBitrate [8] = { 320000, 320000, 320000, 320000, 320000, 320000, 320000, 96000 };
  const char* kPathNames[8] =  { "none", "one", "two", "bbc_radio_three", "bbc_radio_fourfm", "five", "bbc_6music", "bbc_two_hd" };
  const char* kChanNames[8] =  { "none", "one", "two", "bbcRadio3",       "bbcRadio4",        "five", "bbcRadio6",  "bbc2" };
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
