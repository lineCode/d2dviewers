// iPlayer.h
#pragma once

class cVolume {
public:
  cVolume() : mVolume(80) {}
  virtual ~cVolume() {}

  //{{{
  int getVolume() {
    return mVolume;
    }
  //}}}
  //{{{
  void setVolume (int volume) {
    mVolume = volume;
    }
  //}}}

private:
  int mVolume;
  };
