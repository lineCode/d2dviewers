// cRadioChan.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
#include "cHttp.h"
//}}}
#define kMaxChannels 10

class cRadioChan {
public:
  // baseProfile:0   688000:25:640x360    281000:25:384x216   156000:25:256x144    86000:25:192x108    31000:25:192x108
  // mainProfile:1   437000:25:512x288
  // highProfile:2  5070000:50:1280x720  2812000:50:960x540  1604000:25:960x540   827000:25:704x396
  cRadioChan() : mChannel(0), mBaseSeqNum(0), mVidBitrate(1604000), mVidFramesPerChunk(200), mVidProfile(2) {}
  virtual ~cRadioChan() {}

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
