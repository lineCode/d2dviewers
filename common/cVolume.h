// cVolume.h
#pragma once

class cVolume {
public:
  cVolume() {}
  virtual ~cVolume() {}

  float getVolume() {
    return mVolume;
    }

  void setVolume (float volume) {
    if (volume < 0)
      mVolume = 0;
    else if (volume > 100.0f)
      mVolume = 100.0f;
    else
      mVolume = volume;
    }

private:
  float mVolume = 1.0f;
  };
