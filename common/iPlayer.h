// iPlayer.h
#pragma once

#include "cHttp.h"
class cYuvFrame;

class iPlayer {
public:
  virtual ~iPlayer() {}

  // generic interface
  virtual int getAudSampleRate() = 0;
  virtual double getSecsPerAudFrame() = 0;
  virtual double getSecsPerVidFrame() = 0;
  virtual std::string getInfoStr (double frame) = 0;

  virtual bool getPlaying() = 0;
  virtual double getPlaySecs() = 0;

  virtual int getSource() = 0;
  virtual int getNumSource() = 0;
  virtual std::string getSourceStr (int index) = 0;
  virtual void incSourceBitrate() = 0;
  virtual double changeSource (cHttp* http, int channel) = 0;

  virtual void setPlaySecs (double secs) = 0;
  virtual void incPlaySecs (double secs) = 0;
  virtual void incAlignPlaySecs (double secs) = 0;

  virtual void setPlaying (bool playing) = 0;
  virtual void togglePlaying() = 0;

  virtual bool load (ID2D1DeviceContext* dc, cHttp* http, double frame) = 0;

  virtual uint8_t* getPower (double secs, int& frames) = 0;
  virtual int16_t* getAudSamples (double secs, int& seqNum) = 0;
  virtual cYuvFrame* getVidFrame (double secs, int seqNum) = 0;
  };
