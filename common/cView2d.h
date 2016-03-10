// cView2d.h
#pragma once

class cView2d {
public:
  //{{{
  cView2d() : mMatrix(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f), mMinScale(1.0f),
              mSrcSize(SizeU(1, 1)), mDstSize(SizeF(1.0f, 1.0f)) {}
  //}}}

  // gets
  float getScale() { return mMatrix._11; }
  D2D1_POINT_2F getPoint() { return Point2F (mMatrix._31, mMatrix._32); }
  Matrix3x2F& getMatrix() { return mMatrix; }

  // sets
  //{{{
  bool setPoint (float x, float y) {

    bool changed = false;

    float xMin = mDstSize.width - (mSrcSize.width * mMatrix._11);
    if (x > 0.0f) {
      mMatrix._31 = 0.0f;
      changed = true;
      }
    else if (x < xMin) {
      mMatrix._31 = xMin;
      changed = true;
      }
    else if (x != mMatrix._31) {
      mMatrix._31 = x;
      changed = true;
      }

    float yMin = mDstSize.height - (mSrcSize.height * mMatrix._22);
    if (yMin > 0.0f)
      yMin = 0;

    if (y > 0.0f) {
      mMatrix._32 = 0.0f;
      changed = true;
      }
    else if (y < yMin) {
      mMatrix._32 = yMin;
      changed = true;
      }
    else if (y != mMatrix._32) {
      mMatrix._32 = y;
      changed = true;
      }

    return changed;
    }
  //}}}
  //{{{
  void setSrcDstSize (D2D1_SIZE_U srcSize, D2D1_SIZE_F dstSize) {

    auto wasShowingAll = getShowAll();

    mSrcSize = srcSize;
    mDstSize = dstSize;

    float xScale = dstSize.height / srcSize.height;
    float yScale = dstSize.width / srcSize.width;
    mMinScale = (xScale < yScale) ? xScale : yScale;

    if (wasShowingAll)
      showAll();
    }
  //}}}

  bool changeScale (float ratio) { return setScale (getScale() * ratio); }

  // reverse transform
  //{{{
  D2D1_POINT_2F dstToSrc (int x, int y) {

    return Point2F ((x - mMatrix._31) / mMatrix._11, (y - mMatrix._32) / mMatrix._22);
    }
  //}}}
  //{{{
  D2D1_POINT_2F srcToDst (float x, float y) {

    return Point2F ((x * mMatrix._11) + mMatrix._31,  (y * mMatrix._22) + mMatrix._32);
    }
  //}}}

private:
  float getMinScale() { return mMinScale; }
  bool getShowAll() { return getScale() == getMinScale(); }

  //{{{
  bool setScale (float scale) {

    bool changed = false;

    if (scale < mMinScale) {
      mMatrix._11 = mMinScale;
      mMatrix._22 = mMinScale;
      setPoint (mMatrix._31, mMatrix._32);
      changed = true;
      }

    else if ((scale != mMatrix._11) || (scale != mMatrix._22)) {
      mMatrix._11 = scale;
      mMatrix._22 = scale;
      setPoint (mMatrix._31, mMatrix._32);
      changed = true;
      }

    return changed;
    }
  //}}}

  void showAll() { setScale (getMinScale()); }

  // vars
  Matrix3x2F mMatrix;

  float mMinScale;
  D2D1_SIZE_U mSrcSize;
  D2D1_SIZE_F mDstSize;
  };
