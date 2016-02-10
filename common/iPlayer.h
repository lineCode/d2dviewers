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
  virtual std::string getInfoStr (int frame) = 0;
  virtual std::string getFrameStr (int frame) = 0;
  virtual int getPlayFrame() = 0;
  virtual bool getPlaying() = 0;
  virtual int getSource() = 0;
  virtual int getNumSource() = 0;
  virtual std::string getSourceStr (int index) = 0;

  virtual void setPlayFrame (int frame) = 0;
  virtual void incPlayFrame (int inc) = 0;
  virtual void incAlignPlayFrame (int inc) = 0;

  virtual void setPlaying (bool playing) = 0;
  virtual void togglePlaying() = 0;

  virtual int changeSource (cHttp* http, int channel) = 0;
  virtual bool load (ID2D1DeviceContext* dc, cHttp* http, int frame) = 0;

  virtual uint8_t* getPower (int frame, int& frames) = 0;
  virtual int16_t* getAudioSamples (int frame, int& seqNum) = 0;
  virtual cYuvFrame* getVideoFrame (int frame, int seqNum) = 0;
  };
