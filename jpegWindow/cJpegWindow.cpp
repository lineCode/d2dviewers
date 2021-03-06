// cJpegWindow.cpp
//{{{  includes
#include "stdafx.h"

#include "../../shared/utils/utils.h"
#include "../../shared/d2dWindow/cD2DWindow.h"

#include "../common/cView2d.h"
#include "../common/cJpegImage.h"

using namespace date;
using namespace std::chrono;
//}}}
//{{{  typedef
class cJpegWindow;
typedef void (cJpegWindow::*cJpegWindowFunc)();
typedef void (cJpegWindow::*cJpegWindowImageFunc)(cJpegImage* image);
//}}}
//{{{
class cJpegFiles {
public:
  cJpegFiles() {}

  //{{{
  void scan (wstring& parentName, wchar_t* directoryName, wchar_t* pathMatchName,
             int& numImages, int& numDirs, cJpegWindow* jpegWindow, cJpegWindowFunc func) {
  // directoryName is findFileData.cFileName wchar_t*

    mDirName = directoryName;
    mFullDirName = parentName.empty() ? directoryName : parentName + L"\\" + directoryName;
    auto searchStr (mFullDirName +  L"\\*");

    WIN32_FIND_DATA findFileData;
    auto file = FindFirstFileEx (searchStr.c_str(), FindExInfoBasic, &findFileData,
                                 FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (file != INVALID_HANDLE_VALUE) {
      do {
        if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (findFileData.cFileName[0] != L'.'))  {
          auto directory = new cJpegFiles();
          directory->scan (mFullDirName, findFileData.cFileName, pathMatchName, numImages, numDirs, jpegWindow, func);
          mDirectories.push_back (directory);
          }
        else if (PathMatchSpec (findFileData.cFileName, pathMatchName))
          mImages.push_back (new cJpegImage (mFullDirName, findFileData.cFileName));
        } while (FindNextFile (file, &findFileData));
      FindClose (file);
      }

    numImages += (int)mImages.size();
    numDirs += (int)mDirectories.size();

    //printf ("%d %d %ls %ls\n", (int)mImages.size(), (int)mDirectories.size(), mName, mFullName);
    (jpegWindow->*func)();
    }
  //}}}

  //{{{
  cJpegImage* pickImage (D2D1_POINT_2F& point) {

    for (auto directory : mDirectories) {
      auto pickedImage = directory->pickImage (point);
      if (pickedImage)
        return pickedImage;
      }

    for (auto image : mImages)
      if (image->pick (point))
        return image;

    return NULL;
    }
  //}}}
  //{{{
  void traverseImages (cJpegWindow* jpegWindow, cJpegWindowImageFunc imageFunc) {

    for (auto directory : mDirectories)
      directory->traverseImages (jpegWindow, imageFunc);

    for (auto image : mImages)
      (jpegWindow->*imageFunc)(image);
    }
  //}}}
  //{{{
  int bestThumb (D2D1_POINT_2F& point, cJpegImage** bestImage, float& bestMetric) {
  // get bestThumbImage for thumbLoader, nearest unloaded to point

    auto numLoaded = 0;
    for (auto directory : mDirectories)
      numLoaded += directory->bestThumb (point, bestImage, bestMetric);

    for (auto image : mImages)
      if (image->getNoThumb()) {
        auto xdiff = image->getLayout().left - point.x;
        auto ydiff = image->getLayout().top - point.y;
        if ((xdiff >= 0) && (ydiff >= 0)) {
          // only bottom right quadrant
          float metric = (xdiff*xdiff) + (ydiff*ydiff);
          if (metric < bestMetric) {
            bestMetric = metric;
            *bestImage = image;
            }
          }
        }
      else
        numLoaded++;

    return numLoaded;
    }
  //}}}
  //{{{
  void simpleLayoutThumbs (D2D1_RECT_F& rect, D2D1_SIZE_U& thumbSize, int& column, int columns, int rows) {

    for (auto directory : mDirectories)
      directory->simpleLayoutThumbs (rect, thumbSize, column, columns, rows);

    for (auto image : mImages) {
      rect.right = rect.left + thumbSize.width;
      rect.bottom = rect.top + thumbSize.height;
      image->setLayout (rect);

      rect.left = rect.right;
      if ((column++ % columns) == (columns-1)) {
        rect.left = 0;
        rect.top = rect.bottom;
        column = 0;
        }
      }
    }
  //}}}

private:
  wstring mDirName;
  wstring mFullDirName;
  concurrency::concurrent_vector<cJpegImage*> mImages;
  concurrency::concurrent_vector<cJpegFiles*> mDirectories;
  };
//}}}

class cJpegWindow : public cD2dWindow, public cJpegFiles {
public:
  cJpegWindow() : mCurView(&mThumbView) {}
  //{{{
  void run (int width, int height, wchar_t* windowTitle, wchar_t* rootDirectory, int numLoaderThreads) {
  // windowTitle, rootDirectory wchar_t* rather than wstring

    initialise (windowTitle, width, height);
    getDeviceContext()->CreateSolidColorBrush (ColorF (0x101010, 0.8f), &mPanelBrush);

    // start threads
    thread ([=]() { fileScanner (rootDirectory); } ).detach();
    for (auto i = 0; i < numLoaderThreads; i++)
      thread ([=]() { thumbLoader (i); } ).detach();

    messagePump();
    };
  //}}}

protected:
  //{{{
  bool onKey (int key) {

    switch (key) {
      case 0x10: // shift
      case 0x11: // control
      case 0x00: return false;

      case 0x1B: return true; // escape abort

      case 0x20: break; // space bar

      case 0x21: break; // page up
      case 0x22: break; // page down

      case 0x23: break; // end
      case 0x24: break; // home

      case 0x25: break; // left arrow
      case 0x27: break; // right arrow

      case 0x26: if (mCurView->changeScale (1.0f / 1.1f)) changed(); break; // up arrow
      case 0x28: if (mCurView->changeScale (1.1f)) changed(); break; // down arrow

      default: printf ("key %x\n", key);
      }

    return false;
    }
  //}}}
  //{{{
  void onMouseWheel (int delta) {

    auto ratio = mControlKeyDown ? 1.5f : mShiftKeyDown ? 1.25f : 1.1f;
    if (delta < 0)
      ratio = 1.0f / ratio;

    if (mCurView->changeScale (ratio))
      changed();
    }
  //}}}
  //{{{
  void onMouseProx (bool inClient, int x, int y) {

    if (!mFullImage) {
      // displaying thumbs
      mProxImage = inClient ? pickImage (mThumbView.dstToSrc (x, y)) : nullptr;
      if (mPickImage != mProxImage) {
        mPickImage = mProxImage;
        if (mProxImage)
          if (!mProxImage->getThumbBitmap())
            mProxImage->loadInfo();
        changed();
        }
      }
    }
  //}}}
  //{{{
  void onMouseDown (bool right, int x, int y) {
    }
  //}}}
  //{{{
  void onMouseMove (bool right, int x, int y, int xInc, int yInc) {

    auto ratio = mControlKeyDown ? 2.0f : mShiftKeyDown ? 1.5f : 1.0f;

    if (mCurView->setPoint (mCurView->getPoint().x + xInc*ratio,  mCurView->getPoint().y + yInc*ratio))
      changed();
    }
  //}}}
  //{{{
  void onMouseUp (bool right, bool mouseMoved, int x, int y) {

    if (!mouseMoved) {
      if (mFullImage) {
        // full
        mFullImage->releaseFullBitmap();
        mFullImage = nullptr;
        mCurView = &mThumbView;
        changed();
        }

      else if (mPickImage) {
        // thumbs
        setChangeRate (4);
        mFullImage = mPickImage;
        int scale = 1 + int((mFullImage->getImageSize().width / getClientF().width));
        mFullView.setSrcDstSize (SizeU(mFullImage->getImageSize().width/scale, mFullImage->getImageSize().height/scale), getClientF());
        mPickImage->loadFullBitmap (getDeviceContext(), getBitmapProperties(), scale);
        setChangeRate (0);

        changed();
        mCurView = &mFullView;
        }
      }
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    dc->Clear (ColorF(ColorF::Black));

    if (!mFullImage) {
      // thumbs
      dc->SetTransform (mThumbView.getMatrix());
      traverseImages (this, &cJpegWindow::drawThumb);

      if (mPickImage) {
        //{{{  highlight pickImage and draw infoPanel
        dc->DrawRoundedRectangle (RoundedRect (mPickImage->getLayout(), 3.0f,3.0f), getYellowBrush(), 3.0f/mThumbView.getScale());

        // draw info panel
        dc->SetTransform (Matrix3x2F::Scale (Size(1.0f,1.0f), Point2F (0,0)));

        auto rPanel (RectF (0.0f, getClientF().height-180.0f, getClientF().width, getClientF().height));
        dc->FillRectangle (rPanel, mPanelBrush);

        rPanel.left += 10.0f;
        rPanel.top += 10.0f;
        if (mPickImage->getThumbBitmap()) {
          //{{{  draw thumb bitmap
          rPanel.right = rPanel.left + (160.0f*4.0f/3.0f);
          rPanel.bottom = rPanel.top + (120.0f*4.0f/3.0f);

          dc->DrawBitmap (mPickImage->getThumbBitmap(), rPanel);
          rPanel.left = rPanel.right + 10.0f;
          rPanel.right = getClientF().width;
          rPanel.bottom = getClientF().height;
          }
          //}}}

        //{{{  draw fullFileName text
        dc->DrawText (mPickImage->getFullFileName().data(), (uint32_t)mPickImage->getFullFileName().size(), getTextFormat(),
                      rPanel, getWhiteBrush());
        //}}}
        if (!mPickImage->getExifTimeString().empty()) {
          //{{{  draw exifTime text
          rPanel.top += 20.0f;

          dc->DrawText (mPickImage->getExifTimeString().data(), (uint32_t)mPickImage->getExifTimeString().size(), getTextFormat(),
                        rPanel, getWhiteBrush());
          }
          //}}}
        //{{{  draw imageSize text
        rPanel.top += 20.0f;
        wstring wstr (to_wstring (mPickImage->getImageSize().width) + L"x" + to_wstring (mPickImage->getImageSize().height));
        dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), rPanel, getWhiteBrush());
        //}}}
        //{{{  draw make text
        rPanel.top += 20.0f;

        dc->DrawText (mPickImage->getMake().data(), (uint32_t)mPickImage->getMake().size(), getTextFormat(),
                      rPanel, getWhiteBrush());
        //}}}
        //{{{  draw model text
        rPanel.top += 20.0f;

        dc->DrawText (mPickImage->getModel().data(), (uint32_t)mPickImage->getModel().size(), getTextFormat(),
                      rPanel, getWhiteBrush());
        //}}}
        if (false) {
          //{{{  draw orientation text
          rPanel.top += 20.0f;

          wstringstream str;
          str << L"orientation " << mPickImage->getOrientation();
          dc->DrawText (str.str().data(), (uint32_t)str.str().size(), getTextFormat(), rPanel, getWhiteBrush());
          }
          //}}}
        if (mPickImage->getAperture() > 0) {
          //{{{  draw aperture text
          rPanel.top += 20.0f;

          wstring wstr (L"aperture " + to_wstring(mPickImage->getAperture()));
          dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), rPanel, getWhiteBrush());
          }
          //}}}
        if (mPickImage->getFocalLength() > 0) {
          //{{{  draw focalLength text
          rPanel.top += 20.0f;

          wstring wstr (L"focal length " + to_wstring(mPickImage->getFocalLength()));
          dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), rPanel, getWhiteBrush());
          }
          //}}}
        if (mPickImage->getExposure() > 0) {
          //{{{  draw exposure text
          rPanel.top += 20.0f;

          wstring wstr (L"exposure " + to_wstring(mPickImage->getExposure()));
          dc->DrawText (wstr.data(), (uint32_t)wstr.size(), getTextFormat(), rPanel, getWhiteBrush());
          }
          //}}}

        rPanel.left = getClientF().width/2.0f;
        rPanel.top = getClientF().height-180.0f + 10.0f;
        if (true)
          //{{{  draw debugStr
          dc->DrawText (mPickImage->getDebugInfo().data(), (uint32_t)mPickImage->getDebugInfo().size(), getTextFormat(),
                        rPanel, getYellowBrush());

          rPanel.top += 20.0f;
          dc->DrawText (mPickImage->getDebugInfo1().c_str(), (uint32_t)mPickImage->getDebugInfo1().size(), getTextFormat(),
                        rPanel, getYellowBrush());
          //}}}

        //{{{  draw creationTime
        rPanel.top += 20.0f;
        dc->DrawText (mPickImage->getCreationTime().data(), (uint32_t)mPickImage->getCreationTime().size(), getTextFormat(),
                      rPanel, getYellowBrush());
        //}}}
        //{{{  draw lastAccessTime
        rPanel.top += 20.0f;
        dc->DrawText (mPickImage->getLastAccessTime().data(), (uint32_t)mPickImage->getLastAccessTime().size(), getTextFormat(),
                      rPanel, getYellowBrush());
        //}}}
        //{{{  draw lastWriteTime
        rPanel.top += 20.0f;
        dc->DrawText (mPickImage->getLastWriteTime().data(), (uint32_t)mPickImage->getLastWriteTime().size(), getTextFormat(),
                      rPanel, getYellowBrush());
        //}}}

        if (!mPickImage->getGPSinfo().empty()) {
          //{{{  draw GPSinfo
          rPanel.top += 20.0f;
          dc->DrawText (mPickImage->getGPSinfo().data(), (uint32_t)mPickImage->getGPSinfo().size(), getTextFormat(),
                        rPanel, getYellowBrush());
          }
          //}}}

        //{{{  debug text
        wstringstream str;
        str << mNumThumbsLoaded << L":" << mNumNestedImages
            << L" sub:" << mNumNestedDirs
            << L" scale:" << mCurView->getScale()
            << L" point:" << mCurView->getPoint().x << L"," << mCurView->getPoint().y;
        dc->DrawText (str.str().data(), (uint32_t)str.str().size(), getTextFormat(),
                      RectF(getClientF().width/2.0f, getClientF().height-20.0f, getClientF().width, getClientF().height),
                      getWhiteBrush());
        //}}}
        }
        //}}}
      }

    else if (mFullImage->getFullBitmap()) {
      // fullImage
      dc->SetTransform (mFullView.getMatrix());
      dc->DrawBitmap (
        mFullImage->getFullBitmap(),
        RectF (0,0, (float)mFullImage->getFullSize().width,(float)mFullImage->getFullSize().height));

      // full image text
      dc->SetTransform (Matrix3x2F::Scale (Size(1.0f,1.0f), Point2F (0,0)));
      dc->DrawText (mFullImage->getFullFileName().data(), (uint32_t)mFullImage->getFullFileName().size(), getTextFormat(),
                    RectF(getClientF().width/2.0f, 0, getClientF().width,getClientF().height),
                    getWhiteBrush());
      }
    }
  //}}}

private:
  //{{{
  void drawThumb (cJpegImage* image) {

    if (image->getThumbBitmap())
      getDeviceContext()->DrawBitmap (image->getThumbBitmap(), image->getLayout());
    }
  //}}}
  //{{{
  void layoutThumbs() {

    auto windowAspect = getClientF().width / getClientF().height;
    auto thumbAspect = (float)mThumbSize.width / (float)mThumbSize.height;

    auto rows = 1;
    auto columns  = (int)sqrt (mNumNestedImages);
    while (columns < mNumNestedImages) {
      rows = ((mNumNestedImages-1) / columns) + 1;
      if (((thumbAspect * columns) / rows) > windowAspect)
        break;
      columns++;
      }

    int layoutColumn = 0;
    simpleLayoutThumbs (RectF (0,0,0,0), mThumbSize, layoutColumn, columns, rows);

    mThumbView.setSrcDstSize (SizeU(columns * mThumbSize.width, rows * mThumbSize.height), getClientF());

    changed();
    }
  //}}}
  //{{{
  void fileScanner (wchar_t* rootDir) {
  // rootdirectory wchar_t* rather than wstring

    auto time1 = getTimer();
    scan (wstring(), rootDir, L"*.jpg", mNumNestedImages, mNumNestedDirs, this, &cJpegWindow::layoutThumbs);
    mFileSystemScanned = true;
    auto time2 = getTimer();

    wcout << L"scanDirectoryFunc exit images:" << mNumNestedImages
          << L" directories:" << mNumNestedDirs
          << L" took:" << time2-time1
          << endl;
    }
  //}}}
  //{{{
  void thumbLoader (int index) {

    auto slept = 0;

    // give scanner a chance to get going
    while (!mFileSystemScanned && (slept < 10))
      Sleep (slept++);

    auto wasted = 0;
    while (slept < 20) {
      cJpegImage* bestThumbImage = nullptr;
      auto bestMetric = FLT_MAX;
      mNumThumbsLoaded = bestThumb (mThumbView.dstToSrc(0,0), &bestThumbImage, bestMetric);
      if (bestThumbImage) {
        if (bestThumbImage->getNoThumb()) {
          if (bestThumbImage->loadThumbBitmap (getDeviceContext(), getBitmapProperties(), mThumbSize))
            changed();
          }
        else
          wasted++;
        }
      else
        Sleep (++slept);
      }

    wcout << L"thumbLoaderFunc:" << index
          << L" loaded:" << mNumThumbsLoaded
          << L" images:" << mNumNestedImages
          << L" wasted:" << wasted
          << L" slept:" << slept
          << endl;
    }
  //}}}
  //{{{  vars
  int mNumNestedDirs = 0;
  int mNumNestedImages = 0;

  cJpegImage* mProxImage = nullptr;
  cJpegImage* mPickImage = nullptr;
  cJpegImage* mFullImage = nullptr;

  cView2d* mCurView = nullptr;
  cView2d mFullView;
  cView2d mThumbView;

  D2D1_SIZE_U mThumbSize = {160, 120};
  int mNumThumbsLoaded = 0;
  bool mFileSystemScanned = false;

  ID2D1SolidColorBrush* mPanelBrush = nullptr;
  //}}}
  };

//{{{
int wmain (int argc, wchar_t* argv[]) {

  auto today = floor<days>(system_clock::now());
  cout << today << '\n';

  startTimer();

  cJpegWindow jpegWindow;
  jpegWindow.run (1860, 1000, L"jpegView", argv[1] ? argv[1] : L"C:\\Users\\colin\\Pictures", 2);
  }
//}}}
