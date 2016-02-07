// cPlayer.h
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
    if (volume < 0)
      mVolume = 0;
    else if (volume > 100)
      mVolume = 100;
    else
      mVolume = volume;
    }
  //}}}

private:
  int mVolume;
  };
