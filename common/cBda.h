// cBda.h
#pragma once
//{{{  includes
#include <initguid.h>
#include <DShow.h>
#include <bdaiface.h>
#include <ks.h>
#include <ksmedia.h>
#include <bdamedia.h>
#include <bdatif.h>

MIDL_INTERFACE ("0579154A-2B53-4994-B0D0-E773148EFF85")
ISampleGrabberCB : public IUnknown {
public:
  virtual HRESULT STDMETHODCALLTYPE SampleCB (double SampleTime, IMediaSample* pSample) = 0;
  virtual HRESULT STDMETHODCALLTYPE BufferCB (double SampleTime, BYTE* pBuffer, long BufferLen) = 0;
  };

MIDL_INTERFACE ("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown {
public:
  virtual HRESULT STDMETHODCALLTYPE SetOneShot (BOOL OneShot) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetMediaType (const AM_MEDIA_TYPE* pType) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType (AM_MEDIA_TYPE* pType) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetBufferSamples (BOOL BufferThem) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer (long* pBufferSize, long* pBuffer) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetCurrentSample (IMediaSample** ppSample) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetCallback (ISampleGrabberCB* pCallback, long WhichMethodToCallback) = 0;
  };
EXTERN_C const CLSID CLSID_SampleGrabber;

DEFINE_GUID (CLSID_DVBTLocator, 0x9CD64701, 0xBDF3, 0x4d14, 0x8E,0x03, 0xF1,0x29,0x83,0xD8,0x66,0x64);
DEFINE_GUID (CLSID_BDAtif, 0xFC772ab0, 0x0c7f, 0x11d3, 0x8F,0xf2, 0x00,0xa0,0xc9,0x22,0x4c,0xf4);

#pragma comment (lib,"strmiids")

using namespace Microsoft::WRL;
//}}}

//{{{
class cBipBuffer {
public:
    cBipBuffer() : mBuffer(NULL), ixa(0), sza(0), ixb(0), szb(0), buflen(0), ixResrv(0), szResrv(0) {}
    //{{{
    ~cBipBuffer() {
        // We don't call FreeBuffer, because we don't need to reset our variables - our object is dying
        if (mBuffer != NULL)
            ::VirtualFree (mBuffer, buflen, MEM_DECOMMIT);
    }
    //}}}

    //{{{
    bool allocateBuffer (int buffersize) {
    // Allocates a buffer in virtual memory, to the nearest page size (rounded up)
    //   int buffersize                size of buffer to allocate, in bytes (default: 4096)
    //   return bool                        true if successful, false if buffer cannot be allocated

      if (buffersize <= 0)
        return false;
      if (mBuffer != NULL)
        freeBuffer();

      SYSTEM_INFO si;
      ::GetSystemInfo(&si);

      // Calculate nearest page size
      buffersize = ((buffersize + si.dwPageSize - 1) / si.dwPageSize) * si.dwPageSize;

      mBuffer = (BYTE*)::VirtualAlloc(NULL, buffersize, MEM_COMMIT, PAGE_READWRITE);
      if (mBuffer == NULL)
        return false;

      buflen = buffersize;
      return true;
      }
    //}}}
    //{{{
    void clear() {
    // Clears the buffer of any allocations or reservations. Note; it
    // does not wipe the buffer memory; it merely resets all pointers,
    // returning the buffer to a completely empty state ready for new
    // allocations.

      ixa = sza = ixb = szb = ixResrv = szResrv = 0;
      }
    //}}}
    //{{{
    void freeBuffer() {
    // Frees a previously allocated buffer, resetting all internal pointers to 0.

      if (mBuffer == NULL)
        return;

      ixa = sza = ixb = szb = buflen = 0;

      ::VirtualFree(mBuffer, buflen, MEM_DECOMMIT);

      mBuffer = NULL;
      }
    //}}}

    //{{{
    uint8_t* reserve (int size, OUT int& reserved) {
    // Reserves space in the buffer for a memory write operation
    //   int size                 amount of space to reserve
    //   OUT int& reserved        size of space actually reserved
    // Returns:
    //   BYTE*                    pointer to the reserved block
    //   Will return NULL for the pointer if no space can be allocated.
    //   Can return any value from 1 to size in reserved.
    //   Will return NULL if a previous reservation has not been committed.

      // We always allocate on B if B exists; this means we have two blocks and our buffer is filling.
      if (szb) {
        int freespace = getBFreeSpace();
        if (size < freespace)
          freespace = size;
        if (freespace == 0)
          return NULL;

        szResrv = freespace;
        reserved = freespace;
        ixResrv = ixb + szb;
        return mBuffer + ixResrv;
        }
      else {
        // Block b does not exist, so we can check if the space AFTER a is bigger than the space
        // before A, and allocate the bigger one.
        int freespace = getSpaceAfterA();
        if (freespace >= ixa) {
          if (freespace == 0)
            return NULL;
          if (size < freespace)
            freespace = size;

          szResrv = freespace;
          reserved = freespace;
          ixResrv = ixa + sza;
          return mBuffer + ixResrv;
          }
        else {
          if (ixa == 0)
            return NULL;
          if (ixa < size)
            size = ixa;
          szResrv = size;
          reserved = size;
          ixResrv = 0;
          return mBuffer;
          }
        }
      }
    //}}}
    //{{{
    void commit (int size) {
    // Commits space that has been written to in the buffer
    // Parameters:
    //   int size                number of bytes to commit
    //   Committing a size > than the reserved size will cause an assert in a debug build;
    //   in a release build, the actual reserved size will be used.
    //   Committing a size < than the reserved size will commit that amount of data, and release
    //   the rest of the space.
    //   Committing a size of 0 will release the reservation.

      if (size == 0) {
        // decommit any reservation
        szResrv = ixResrv = 0;
        return;
        }

      // If we try to commit more space than we asked for, clip to the size we asked for.

      if (size > szResrv)
        size = szResrv;

      // If we have no blocks being used currently, we create one in A.
      if (sza == 0 && szb == 0) {
        ixa = ixResrv;
        sza = size;

        ixResrv = 0;
        szResrv = 0;
        return;
        }

      if (ixResrv == sza + ixa)
        sza += size;
      else
        szb += size;

      ixResrv = 0;
      szResrv = 0;
      }
    //}}}

    //{{{
    uint8_t* getContiguousBlock (OUT int& size) {
    // Gets a pointer to the first contiguous block in the buffer, and returns the size of that block.
    //   OUT int & size            returns the size of the first contiguous block
    // Returns:
    //   BYTE*                    pointer to the first contiguous block, or NULL if empty.

      if (sza == 0) {
        size = 0;
        return NULL;
        }

      size = sza;
      return mBuffer + ixa;
      }
    //}}}
    //{{{
    void decommitBlock (int size) {
    // Decommits space from the first contiguous block
    //   int size                amount of memory to decommit

      if (size >= sza) {
        ixa = ixb;
        sza = szb;
        ixb = 0;
        szb = 0;
        }
      else {
        sza -= size;
        ixa += size;
        }
      }
    //}}}
    //{{{
    int getCommittedSize() const {
    // Queries how much data (in total) has been committed in the buffer
    // Returns:
    //   int                    total amount of committed data in the buffer

      return sza + szb;
      }
    //}}}

private:
  //{{{
  int getSpaceAfterA() const {
    return buflen - ixa - sza;
    }
  //}}}
  //{{{
  int getBFreeSpace() const {
    return ixa - ixb - szb;
    }
  //}}}

  uint8_t* mBuffer;
  int buflen;

  int ixa;
  int sza;

  int ixb;
  int szb;

  int ixResrv;
  int szResrv;
  };
//}}}
//{{{
class cSampleGrabberCB : public ISampleGrabberCB {
public:
  cSampleGrabberCB() {}
  virtual ~cSampleGrabberCB() {}

  //{{{
  void allocateBuffer (int bufSize) {
    mBipBuffer.allocateBuffer (bufSize);
    }
  //}}}

  //{{{
  uint8_t* getContiguousBlock (int& len) {

    return mBipBuffer.getContiguousBlock (len);
    }
  //}}}
  //{{{
  void decommitBlock (int len) {

    mBipBuffer.decommitBlock (len);
    }
  //}}}

  int mPackets = 0;

private:
  // ISampleGrabberCB methods
  STDMETHODIMP_(ULONG) AddRef() { return ++ul_cbrc; }
  STDMETHODIMP_(ULONG) Release() { return --ul_cbrc; }
  STDMETHODIMP QueryInterface (REFIID riid, void** p_p_object) { return E_NOTIMPL; }

  //{{{
  STDMETHODIMP BufferCB (double sampleTime, BYTE* samples, long sampleLen) {
    printf ("cSampleGrabberCB::BufferCB called\n");
    return S_OK;
    }
  //}}}
  //{{{
  STDMETHODIMP SampleCB (double sampleTime, IMediaSample* mediaSample) {

    if (mediaSample->IsDiscontinuity() == S_OK)
      printf ("cSampleGrabCB::SampleCB sample isDiscontinuity\n");

    int actualDataLength = mediaSample->GetActualDataLength();
    if (actualDataLength != 240*188)
      printf ("cSampleGrabCB::SampleCB - unexpected sampleLength\n");

    mPackets++;

    int bytesAllocated = 0;
    uint8_t* ptr = mBipBuffer.reserve (actualDataLength, bytesAllocated);

    if (!ptr || (bytesAllocated != actualDataLength))
      printf ("failed to reserve buffer\n");
    else {
      uint8_t* samples;
      mediaSample->GetPointer (&samples);
      memcpy (ptr, samples, actualDataLength);
      mBipBuffer.commit (actualDataLength);
      }

    return S_OK;
    }
  //}}}

  // vars
  ULONG ul_cbrc = 0;
  cBipBuffer mBipBuffer;
  };
//}}}

class cBda {
public:
  cBda (int bufSize)  { mSampleGrabberCB.allocateBuffer (bufSize); }
  ~cBda() {}
  //{{{
  bool createGraph (int freq) {

    printf ("cBda::createGraph %d\n", freq);
    CoCreateInstance (CLSID_FilterGraph, nullptr,
                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (mGraphBuilder.GetAddressOf()));

    ComPtr<IBaseFilter> dvbtNetworkProvider;
    CoCreateInstance (CLSID_DVBTNetworkProvider, nullptr,
                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (dvbtNetworkProvider.GetAddressOf()));
    mGraphBuilder->AddFilter (dvbtNetworkProvider.Get(), L"dvbtNetworkProvider");

    // scanningTuner interface from dvbtNetworkProvider
    dvbtNetworkProvider.As (&mScanningTuner);

    ComPtr<ITuningSpace> tuningSpace;
    mScanningTuner->get_TuningSpace (tuningSpace.GetAddressOf());

    // setup dvbTuningSpace2 interface
    ComPtr<IDVBTuningSpace2> dvbTuningSpace2;
    tuningSpace.As (&dvbTuningSpace2);
    dvbTuningSpace2->put__NetworkType (CLSID_DVBTNetworkProvider);
    dvbTuningSpace2->put_SystemType (DVB_Terrestrial);
    dvbTuningSpace2->put_NetworkID (9018);
    dvbTuningSpace2->put_FrequencyMapping (L"");
    dvbTuningSpace2->put_UniqueName (L"DTV DVB-T");
    dvbTuningSpace2->put_FriendlyName (L"DTV DVB-T");

    // create dvbtLocator and setup in dvbTuningSpace2 interface
    ComPtr<IDVBTLocator> dvbtLocator;
    CoCreateInstance (CLSID_DVBTLocator, nullptr,
                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (dvbtLocator.GetAddressOf()));
    dvbtLocator->put_CarrierFrequency (freq);
    dvbtLocator->put_Bandwidth (8);
    dvbTuningSpace2->put_DefaultLocator (dvbtLocator.Get());

    // tuneRequest from scanningTuner
    ComPtr<ITuneRequest> tuneRequest;
    HRESULT hr = mScanningTuner->get_TuneRequest (tuneRequest.GetAddressOf());
    if (hr != S_OK)
      hr = tuningSpace->CreateTuneRequest (tuneRequest.GetAddressOf());

    tuneRequest->put_Locator (dvbtLocator.Get());
    mScanningTuner->Validate (tuneRequest.Get());
    mScanningTuner->put_TuneRequest (tuneRequest.Get());

    // dvbtNetworkProvider -> dvbtTuner -> dvbtCapture -> sampleGrabberFilter -> mpeg2Demux -> bdaTif
    ComPtr<IBaseFilter> dvbtTuner  =
      findFilter (mGraphBuilder, KSCATEGORY_BDA_NETWORK_TUNER, L"DVBTtuner", dvbtNetworkProvider);
    if (!dvbtTuner)
      return false;

    ComPtr<IBaseFilter> dvbtCapture =
      findFilter (mGraphBuilder, KSCATEGORY_BDA_RECEIVER_COMPONENT, L"DVBTcapture", dvbtTuner);

    ComPtr<IBaseFilter> sampleGrabberFilter =
      createFilter (mGraphBuilder, CLSID_SampleGrabber, L"grabber", dvbtCapture);
    ComPtr<ISampleGrabber> sampleGrabber;
    sampleGrabberFilter.As (&sampleGrabber);
    sampleGrabber->SetOneShot (false);
    sampleGrabber->SetBufferSamples (true);
    sampleGrabber->SetCallback (&mSampleGrabberCB, 0);

    ComPtr<IBaseFilter> mpeg2Demux =
      createFilter (mGraphBuilder, CLSID_MPEG2Demultiplexer, L"MPEG2demux", sampleGrabberFilter);
    ComPtr<IBaseFilter> bdaTif =
      createFilter (mGraphBuilder, CLSID_BDAtif, L"BDAtif", mpeg2Demux);

    ComPtr<IMediaControl> mediaControl;
    mGraphBuilder.As (&mediaControl);
    mediaControl->Run();

    printf ("- running\n");
    return true;
    }
  //}}}

  int getPackets() { return mSampleGrabberCB.mPackets; }
  //{{{
  int getSignalStrength() {

    long strength = 0;

    if (mScanningTuner)
      mScanningTuner->get_SignalStrength (&strength);

    return strength / 100000;
    }
  //}}}
  //{{{
  uint8_t* getContiguousBlock (int& len) {
    return mSampleGrabberCB.getContiguousBlock (len);
    }
  //}}}
  //{{{
  void decommitBlock (int len) {
    mSampleGrabberCB.decommitBlock (len);
    }
  //}}}

private:
  //{{{
  bool connectPins (ComPtr<IGraphBuilder> graphBuilder,
                     ComPtr<IBaseFilter> fromFilter, ComPtr<IBaseFilter> toFilter,
                     wchar_t* fromPinName = NULL, wchar_t* toPinName = NULL) {
  // connect pins, if name == NULL use first correct direction unconnected pin, else use pin matching name

    ComPtr<IEnumPins> fromPins;
    fromFilter->EnumPins (&fromPins);

    ComPtr<IPin> fromPin;
    while (fromPins->Next (1, &fromPin, NULL) == S_OK) {
      ComPtr<IPin> connectedPin;
      fromPin->ConnectedTo (&connectedPin);
      if (!connectedPin) {
        // match fromPin info
        PIN_INFO fromPinInfo;
        fromPin->QueryPinInfo (&fromPinInfo);
        if ((fromPinInfo.dir == PINDIR_OUTPUT) && (!fromPinName || !wcscmp (fromPinInfo.achName, fromPinName))) {
          // found fromPin, look for toPin
          ComPtr<IEnumPins> toPins;
          toFilter->EnumPins (&toPins);

          ComPtr<IPin> toPin;
          while (toPins->Next (1, &toPin, NULL) == S_OK) {
            ComPtr<IPin> connectedPin;
            toPin->ConnectedTo (&connectedPin);
            if (!connectedPin) {
              // match toPin info
              PIN_INFO toPinInfo;
              toPin->QueryPinInfo (&toPinInfo);
              if ((toPinInfo.dir == PINDIR_INPUT) && (!toPinName || !wcscmp (toPinInfo.achName, toPinName))) {
                // found toPin
                if (graphBuilder->Connect(fromPin.Get(), toPin.Get()) == S_OK) {
                  wprintf(L"- connecting pin %s to %s\n", fromPinInfo.achName, toPinInfo.achName);
                  return true;
                  }
                else {
                  printf ("- connectPins failed\n");
                  return false;
                  }
                }
              }
            }
          printf ("connectPins - no toPin\n");
          return false;
          }
        }
      }

    printf ("connectPins - no fromPin\n");
    return false;
    }
  //}}}
  //{{{
  ComPtr<IBaseFilter> findFilter (ComPtr<IGraphBuilder> graphBuilder, const CLSID& clsid, wchar_t* name,
                                         ComPtr<IBaseFilter> fromFilter) {
  // Find instance of filter of type CLSID by name, add to graphBuilder, connect fromFilter

    ComPtr<IBaseFilter> filter;

    ComPtr<ICreateDevEnum> systemDevEnum;
    CoCreateInstance (CLSID_SystemDeviceEnum, nullptr,
                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&systemDevEnum));

    ComPtr<IEnumMoniker> classEnumerator;
    systemDevEnum->CreateClassEnumerator (clsid, &classEnumerator, 0);

    if (classEnumerator) {
      int i = 1;
      IMoniker* moniker = NULL;
      ULONG fetched;
      while (classEnumerator->Next (1, &moniker, &fetched) == S_OK) {
        IPropertyBag* propertyBag;
        if (moniker->BindToStorage (NULL, NULL, IID_IPropertyBag, (void**)&propertyBag) == S_OK) {
          VARIANT varName;
          VariantInit (&varName);
          propertyBag->Read (L"FriendlyName", &varName, 0);
          VariantClear (&varName);

          wprintf (L"FindFilter - %s\n", varName.bstrVal);

          // bind the filter
          moniker->BindToObject (NULL, NULL, IID_IBaseFilter, (void**)(&filter));

          graphBuilder->AddFilter (filter.Get(), name);
          propertyBag->Release();

          if (connectPins (graphBuilder, fromFilter, filter)) {
            propertyBag->Release();
            break;
            }
          else
            graphBuilder->RemoveFilter (filter.Get());
          }
        propertyBag->Release();
        moniker->Release();
        }

      moniker->Release();
      }
    return filter;
    }
  //}}}
  //{{{
  ComPtr<IBaseFilter> createFilter (ComPtr<IGraphBuilder> graphBuilder, const CLSID& clsid,
                                           wchar_t* title, ComPtr<IBaseFilter> fromFilter) {
  // createFilter of type clsid, add to graphBuilder, connect fromFilter

    ComPtr<IBaseFilter> filter;
    CoCreateInstance (clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&filter));

    graphBuilder->AddFilter (filter.Get(), title);
    connectPins (graphBuilder, fromFilter, filter);

    wprintf (L"CreateFilter %s\n", title);

    return filter;
    }
  //}}}

  // vars
  ComPtr<IGraphBuilder> mGraphBuilder;
  ComPtr<IScanningTuner> mScanningTuner;

  cSampleGrabberCB mSampleGrabberCB;
  };
