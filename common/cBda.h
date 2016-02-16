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

#define BUFSIZE (256*240*188)
//{{{
class cSampleGrabberCB : public ISampleGrabberCB {
public:
  //{{{
  cSampleGrabberCB() {
    mSamples = (uint8_t*)malloc (BUFSIZE);
    mSemaphore = CreateSemaphore (NULL, 0, 1, L"loadSem");  // initial 0, max 1
    }
  //}}}
  //{{{
  virtual ~cSampleGrabberCB() {
    CloseHandle (mSemaphore);
    }
  //}}}
  //{{{
  uint8_t* getSamples (int64_t len) {

    if (mNumSamplesRx - mNumSamplesUsed > BUFSIZE) {
      printf ("cSampleGrabberCB::getSamples buffer stale\n");
      mNumSamplesUsed = mNumSamplesRx - BUFSIZE;
      }
    else {
      while (len > mNumSamplesRx - mNumSamplesUsed) {
        WaitForSingleObject (mSemaphore, 10 * 1000);
        //printf ("wait %lld %lld %lld %lld\n", len, mNumSamplesRx, mNumSamplesUsed, mNumSamplesRx - mNumSamplesUsed);
        }
      }

    uint8_t* ptr = mSamples + (mNumSamplesUsed % BUFSIZE);
    mNumSamplesUsed += len;
    return ptr;
    }
  //}}}

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

    long actualDataLength = mediaSample->GetActualDataLength();
    if (actualDataLength != 240*188)
      printf ("cSampleGrabCB::SampleCB - unexpected sampleLength\n");

    uint8_t* samples;
    mediaSample->GetPointer (&samples);
    memcpy (mSamples + (mNumSamplesRx % BUFSIZE), samples, actualDataLength);
    mNumSamplesRx += actualDataLength;

    ReleaseSemaphore (mSemaphore, 1, NULL);

    mNumCb++;
    return S_OK;
    }
  //}}}

  // vars
  ULONG ul_cbrc;
  HANDLE mSemaphore;

  int mNumCb = 0;
  volatile int64_t mNumSamplesRx = 0;
  volatile int64_t mNumSamplesUsed = 0;

  uint8_t* mSamples;
  };
//}}}

class cBda {
public:
  cBda() {}
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

  //{{{
  int getSignalStrength() {

    long strength = 0;

    if (mScanningTuner)
      mScanningTuner->get_SignalStrength (&strength);

    return strength / 100000;
    }
  //}}}
  //{{{
  uint8_t* getSamples (int len) {
    return mSampleGrabberCB.getSamples (len);
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
