// cRadioChan.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
//}}}
#define kMaxChans 11

class cRadioChan {
public:
  // baseProfile:0   688000:25:640x360    281000:25:384x216   156000:25:256x144    86000:25:192x108    31000:25:192x108
  // mainProfile:1   437000:25:512x288
  // highProfile:2  5070000:50:1280x720  2812000:50:960x540  1604000:25:960x540   827000:25:704x396
  cRadioChan() : mChan(0), mBaseSeqNum(0), mVidBitrate(281000), mVidFramesPerChunk(200), mVidProfile(1) {}
  ~cRadioChan() {}

  // gets
  //{{{
  int getChan() {
    return mChan;
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
  int getAudFramesPerChunk() {
    return kAudFramesPerChunk[getRadioTv()];
    }
  //}}}
  //{{{
  int getVidFramesPerChunk() {
    return mVidFramesPerChunk;
    }
  //}}}
  //{{{
  float getAudFramesPerSecond() {
    return (float)kAudSampleRate / (float)kAudSamplesPerAacFrame;
    }
  //}}}
  //{{{
  int getAudFramesFromSec (int sec) {
    return int (sec * getAudFramesPerSecond());
    }
  //}}}

  //{{{
  int getAudLowBitrate() {
    return kLowBitrate [mChan];
    }
  //}}}
  //{{{
  int getAudMidBitrate() {
    return kMidBitrate [mChan];
    }
  //}}}
  //{{{
  int getAudHighBitrate() {
    return kHighBitrate [mChan];
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
  int getRadioTv() {
    return kRadioTv[mChan];
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
  std::string getChanName (int chan) {
    return kChanNames [chan];
    }
  //}}}
  //{{{
  std::string getDateTime() {
    return mDateTime;
    }
  //}}}
  //{{{
  std::string getChanInfoStr() {
    return mChanInfoStr;
    }
  //}}}

  // set
  //{{{
  void setChan (cHttp* http, int chan) {

    mChan = chan;
    mHost = getRadioTv() ? "vs-hls-uk-live.bbcfmt.vo.llnwd.net" : "as-hls-uk-live.bbcfmt.vo.llnwd.net";

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
  //}}}

private:
  //{{{  const
  const int kAudSampleRate = 48000;
  const int kAudSamplesPerAacFrame = 1024;
  const int kAudFramesPerChunk[2] = { 300, 375 };  // radio:6.4s, tv:8s

  const bool kRadioTv    [kMaxChans] = { false,   false,  false,  false,  false,  false,  false,   true,   true,   true,   true };
  const int kPool        [kMaxChans] = {      0,      7,      7,      7,      6,      6,      6,      4,      5,      2,      3 };
  const int kLowBitrate  [kMaxChans] = {  48000,  48000,  48000,  48000,  48000,  48000,  48000,  96000,  96000,  96000,  96000 };
  const int kMidBitrate  [kMaxChans] = { 128000, 128000, 128000, 128000, 128000, 128000, 128000,  96000,  96000,  96000,  96000 };
  const int kHighBitrate [kMaxChans] = { 320000, 320000, 320000, 320000, 320000, 320000, 320000,  96000,  96000,  96000,  96000 };
  const char* kChanNames [kMaxChans] = { "none", "radio1", "radio2", "radio3", "radio4", "radio5", "radio6", "bbc1", "bbc2", "bbc4", "bbcNews" };
  const char* kPathNames [kMaxChans] = { "none", "bbc_radio_one",    "bbc_radio_two", "bbc_radio_three", "bbc_radio_fourfm",
                                         "bbc_radio_five_live", "bbc_6music",      "bbc_one_hd",        "bbc_two_hd",
                                                 "bbc_four_hd",   "bbc_news_channel_hd" };
  //}}}
  //{{{
  std::string getPathRoot (int audBitrate) {

    std::string path = "pool_" + toString (kPool[mChan]) + "/live/" +
                       kPathNames[mChan] + '/' + kPathNames[mChan] + ".isml/" + kPathNames[mChan];
    if (getRadioTv()) { // tv
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
  int mChan;
  int mBaseSeqNum;
  int mVidBitrate;
  int mVidFramesPerChunk;
  int mVidProfile;
  std::string mHost;
  std::string mDateTime;
  std::string mChanInfoStr;
  };
