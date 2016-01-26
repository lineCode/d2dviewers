// cRadioChan.h
#pragma once
#include "cParsedUrl.h"
#include "cHttp.h"

class cRadioChan {
public:
  cRadioChan() : mChan(0), mBaseSeqNum(0) {}
  ~cRadioChan() {}

  //{{{
  int getBaseSeqNum() {
    return mBaseSeqNum - kOffset;
    }
  //}}}
  //{{{
  int getChan() {
    return mChan;
    }
  //}}}
  //{{{
  int getBitrate() {
    return mBitrate;
    }
  //}}}
  //{{{
  const char* getHost() {
    return mHost;
    }
  //}}}
  //{{{
  const char* getPath (int seqNum) {

    sprintf (mPath, "pool_%d/live/%s/%s.isml/%s-audio=%d-%d.ts",
             kPool[mChan], kChanNames[mChan], kChanNames[mChan], kChanNames[mChan], mBitrate, seqNum + kOffset);
    return mPath;
    }
  //}}}
  //{{{
  const char* getDateTime() {
    return mDateTime;
    }
  //}}}
  //{{{
  const char* getChanName() {
    return kChanNames[mChan];
    }
  //}}}

  //{{{
  void setChan (int chan, int bitrate) {
    mChan = chan;
    mBitrate = bitrate;
    findM3u8SeqNum();
    }
  //}}}

private:
  // const
  const char* kBbcHost = "as-hls-uk-live.bbcfmt.vo.llnwd.net";
  const int kPool [7] = { 0, 0, 0, 7, 6, 0, 6 };
  const char* kChanNames[7] = { "none", "one", "two", "bbc_radio_three", "bbc_radio_fourfm", "five", "bbc_6music" };
  const int kOffset = 220000000;

  //{{{
  void findM3u8SeqNum() {

    cHttp m3u8;

    sprintf (mPath, "pool_%d/live/%s/%s.isml/%s-audio%%3d%d.m3u8",
             kPool[mChan], kChanNames[mChan], kChanNames[mChan], kChanNames[mChan], mBitrate);
    if (m3u8.get (kBbcHost, mPath) == 302) {
      strcpy (mHost, m3u8.getRedirectedHost());
      m3u8.get (mHost, mPath);
      }
    else
      strcpy (mHost, m3u8.getRedirectedHost());

    printf ("%s\n", (char*)m3u8.getContent());

    // find #EXT-X-MEDIA-SEQUENCE in .m3u8, point to seqNum string, extract seqNum from playListBuf
    auto extSeq = strstr ((char*)m3u8.getContent(), "#EXT-X-MEDIA-SEQUENCE:") + strlen ("#EXT-X-MEDIA-SEQUENCE:");
    auto extSeqEnd = strchr (extSeq, '\n');
    *extSeqEnd = '\0';
    mBaseSeqNum = atoi (extSeq) + 2;

    auto extDateTime = strstr (extSeqEnd + 1, "#EXT-X-PROGRAM-DATE-TIME:") + strlen ("#EXT-X-PROGRAM-DATE-TIME:");
    auto extDateTimeEnd = strchr (extDateTime, '\n');
    *extDateTimeEnd = '\0';
    strcpy (mDateTime, extDateTime);
    }
  //}}}

  // vars
  int mBaseSeqNum;
  int mChan;
  int mBitrate;
  char mDateTime[80];
  char mHost[80];
  char mPath[200];
  };
