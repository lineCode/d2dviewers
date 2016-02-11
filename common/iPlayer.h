// iPlayer.h
#pragma once

#include "cHttp.h"
class cYuvFrame;

class iPlayer {
public:
  virtual ~iPlayer() {}

  // generic interface
  virtual int getAudSampleRate() = 0;
  virtual int getAudFramesFromSec (int sec) = 0;
  virtual std::string getInfoStr (double frame) = 0;
  virtual std::string getFrameStr (double frame) = 0;
  virtual double getPlayFrame() = 0;
  virtual bool getPlaying() = 0;
  virtual int getSource() = 0;
  virtual int getNumSource() = 0;
  virtual std::string getSourceStr (int index) = 0;

  virtual void setPlayFrame (double seconds) = 0;
  virtual void incPlayFrame (double seconds) = 0;
  virtual void incAlignPlayFrame (double seconds) = 0;

  virtual void setPlaying (bool playing) = 0;
  virtual void togglePlaying() = 0;

  virtual int changeSource (cHttp* http, int channel) = 0;
  virtual bool load (ID2D1DeviceContext* dc, cHttp* http, double frame) = 0;

  virtual uint8_t* getPower (double seconds, int& frames) = 0;
  virtual int16_t* getAudioSamples (double seconds, int& seqNum) = 0;
  virtual cYuvFrame* getVideoFrame (double seconds, int seqNum) = 0;
  };
