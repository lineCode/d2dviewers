// utils.cpp
//{{{  includes
#include "pch.h"

#include "usbUtils.h"

#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"legacy_stdio_definitions.lib")
#pragma comment(lib,"CyAPI.lib")
//}}}
//{{{  vars
CCyUSBDevice* USBDevice = NULL;
HANDLE Handle = 0;

BYTE outEpAddress = 0x0;
BYTE inEpAddress = 0x0;

CCyControlEndPoint* controlEndPt = NULL;
CCyBulkEndPoint* epBulkOut = NULL;
CCyBulkEndPoint* epBulkIn = NULL;
//}}}

//{{{
void initUSB() {

  USBDevice = new CCyUSBDevice (Handle, CYUSBDRV_GUID, true);
  if (USBDevice) {
    int deviceCount = USBDevice->DeviceCount();
    printf ("USBDevices:%d\n", deviceCount);

    for (auto device = 0; device < deviceCount; device++) {
      USBDevice->Open (device);
      printf ("vid:%04x pid:%04x  AltIntfcCount:%d EndPointCount:%d - %s\n",
              USBDevice->VendorID, USBDevice->ProductID,
              USBDevice->AltIntfcCount(), USBDevice->EndPointCount(), USBDevice->FriendlyName);

      controlEndPt = USBDevice->ControlEndPt;
      printf ("Add:%02x - controlEndpoint - att:%x bIn:%x\n",
              controlEndPt->Address, controlEndPt->Attributes, controlEndPt->bIn);

      for (auto altInterface = 0; altInterface < USBDevice->AltIntfcCount()+1; altInterface++) {
        USBDevice->SetAltIntfc (altInterface);
        int endPointCount = USBDevice->EndPointCount();
        for (int endPoint = 1; endPoint < endPointCount; endPoint++) {
          CCyUSBEndPoint* cyUSBendPoint = USBDevice->EndPoints[endPoint];

          printf ("Add:%02x - altInterface:%d - endpoint:%d - att:%x bIn:%x\n",
                  cyUSBendPoint->Address, altInterface, endPoint, cyUSBendPoint->Attributes, cyUSBendPoint->bIn);
          if (cyUSBendPoint->bIn) {
            inEpAddress = cyUSBendPoint->Address;
            epBulkIn = (CCyBulkEndPoint*)USBDevice->EndPointOf (inEpAddress);
            epBulkIn->TimeOut = 500;
            printf ("- in - MaxPktSize:%d\n", epBulkIn->MaxPktSize );
            }
          else
            outEpAddress = cyUSBendPoint->Address;
          }
        }
      }
    }
  else
    printf ("no USBDevice\n");
  }
//}}}
//{{{
void closeUSB() {

  USBDevice->Close();
  delete USBDevice;
  }
//}}}
//{{{
CCyBulkEndPoint* getBulkEndPoint() {

  return epBulkIn;
  }
//}}}

//{{{
void writeReg (WORD addr, WORD data) {

  //printf ("writeReg (%02x, %04x);\n", addr, data);

  controlEndPt->Target = TGT_DEVICE;
  controlEndPt->ReqType = REQ_VENDOR;
  controlEndPt->Direction = DIR_TO_DEVICE;
  controlEndPt->ReqCode = 0xAE;
  controlEndPt->Value = addr;
  controlEndPt->Index = 0;

  BYTE buf[2];
  buf[0] = data >> 8;
  buf[1] = data & 0xFF;
  LONG buflen = 2;
  controlEndPt->XferData (buf, buflen);
  }
//}}}
//{{{
WORD readReg (WORD addr) {

  controlEndPt->Target = TGT_DEVICE;
  controlEndPt->ReqType = REQ_VENDOR;
  controlEndPt->Direction = DIR_FROM_DEVICE;
  controlEndPt->ReqCode = 0xAD;
  controlEndPt->Value = addr;
  controlEndPt->Index = 0;

  BYTE buf[2];
  LONG buflen = 2;
  controlEndPt->XferData (buf, buflen);
  WORD value = (buf[0] << 8) | buf[1];

  //printf ("%04x = readReg (%02x);\n", value, addr);

  return value;
  }
//}}}

//{{{
void startStreaming() {

  controlEndPt->Target = TGT_DEVICE;
  controlEndPt->ReqType = REQ_VENDOR;
  controlEndPt->Direction = DIR_TO_DEVICE;
  controlEndPt->ReqCode = 0xAF;
  controlEndPt->Value = 0x123;
  controlEndPt->Index = 0;

  BYTE buf[2];
  buf[0] = 0x12;
  buf[1] = 0x34;
  LONG buflen = 2;
  controlEndPt->XferData(buf, buflen);
  }
//}}}
//{{{
void sendFocus (int value) {

  controlEndPt->Target = TGT_DEVICE;
  controlEndPt->ReqType = REQ_VENDOR;
  controlEndPt->Direction = DIR_TO_DEVICE;
  controlEndPt->ReqCode = 0xAC;
  controlEndPt->Value = value;
  controlEndPt->Index = 0;

  BYTE buf[2];
  buf[0] = 0x12;
  buf[1] = 0x34;
  LONG buflen = 2;
  controlEndPt->XferData (buf, buflen);
  }
//}}}

//{{{
void jpeg (int width, int height) {
//{{{  last data byte status ifp page2 0x02
// b:0 = 1  transfer done
// b:1 = 1  output fifo overflow
// b:2 = 1  spoof oversize error
// b:3 = 1  reorder buffer error
// b:5:4    fifo watermark
// b:7:6    quant table 0 to 2
//}}}

  writeReg (0xf0, 0x0001);

  // mode_config JPG bypass - shadow ifp page2 0x0a
  writeReg (0xc6, 0x270b); writeReg (0xc8, 0x0010); // 0x0030 to disable B

  //{{{  jpeg.config id=9  0x07
  // b:0 =1  video
  // b:1 =1  handshake on error
  // b:2 =1  enable retry on error
  // b:3 =1  host indicates ready
  // ------
  // b:4 =1  scaled quant
  // b:5 =1  auto select quant
  // b:6:7   quant table id
  //}}}
  writeReg (0xc6, 0xa907); writeReg (0xc8, 0x0031);

  //{{{  mode fifo_config0_B - shadow ifp page2 0x0d
  //   output config  ifp page2  0x0d
  //   b:0 = 1  enable spoof frame
  //   b:1 = 1  enable pixclk between frames
  //   b:2 = 1  enable pixclk during invalid data
  //   b:3 = 1  enable soi/eoi
  //   -------
  //   b:4 = 1  enable soi/eoi during FV
  //   b:5 = 1  enable ignore spoof height
  //   b:6 = 1  enable variable pixclk
  //   b:7 = 1  enable byteswap
  //   -------
  //   b:8 = 1  enable FV on LV
  //   b:9 = 1  enable status inserted after data
  //   b:10 = 1  enable spoof codes
  //}}}
  writeReg (0xc6, 0x2772); writeReg (0xc8, 0x0067);

  //{{{  mode fifo_config1_B - shadow ifp page2 0x0e
  //   b:3:0   pclk1 divisor
  //   b:7:5   pclk1 slew
  //   -----
  //   b:11:8  pclk2 divisor
  //   b:15:13 pclk2 slew
  //}}}
  writeReg (0xc6, 0x2774); writeReg (0xc8, 0x0101);

  //{{{  mode fifo_config2_B - shadow ifp page2 0x0f
  //   b:3:0   pclk3 divisor
  //   b:7:5   pclk3 slew
  //}}}
  writeReg (0xc6, 0x2776); writeReg (0xc8, 0x0001);

  // mode OUTPUT WIDTH HEIGHT B
  writeReg (0xc6, 0x2707); writeReg (0xc8, width);
  writeReg (0xc6, 0x2709); writeReg (0xc8, height);

  // mode SPOOF WIDTH HEIGHT B
  writeReg (0xc6, 0x2779); writeReg (0xc8, width);
  writeReg (0xc6, 0x277b); writeReg (0xc8, height);

  //writeReg (0x09, 0x000A); // factory bypass 10 bit sensor
  writeReg (0xC6, 0xA120); writeReg (0xC8, 0x02); // Sequencer.params.mode - capture video
  writeReg (0xC6, 0xA103); writeReg (0xC8, 0x02); // Sequencer goto capture B

  //writeReg (0xc6, 0xa907); printf ("JPEG_CONFIG %x\n",readReg (0xc8));
  //writeReg (0xc6, 0x2772); printf ("MODE_FIFO_CONF0_B %x\n",readReg (0xc8));
  //writeReg (0xc6, 0x2774); printf ("MODE_FIFO_CONF1_B %x\n",readReg (0xc8));
  //writeReg (0xc6, 0x2776); printf ("MODE_FIFO_CONF2_B %x\n",readReg (0xc8));
  //writeReg (0xc6, 0x2707); printf ("MODE_OUTPUT_WIDTH_B %d\n",readReg (0xc8));
  //writeReg (0xc6, 0x2709); printf ("MODE_OUTPUT_HEIGHT_B %d\n",readReg (0xc8));
  //writeReg (0xc6, 0x2779); printf ("MODE_SPOOF_WIDTH_B %d\n",readReg (0xc8));
  //writeReg (0xc6, 0x277b); printf ("MODE_SPOOF_HEIGHT_B %d\n",readReg (0xc8));
  //writeReg (0xc6, 0xa906); printf ("JPEG_FORMAT %d\n",readReg (0xc8));
  //writeReg (0xc6, 0x2908); printf ("JPEG_RESTART_INT %d\n", readReg (0xc8));
  //writeReg (0xc6, 0xa90a); printf ("JPEG_QSCALE_1 %d\n",readReg (0xc8));
  //writeReg (0xc6, 0xa90b); printf ("JPEG_QSCALE_2 %d\n",readReg (0xc8));
  //writeReg (0xc6, 0xa90c); printf ("JPEG_QSCALE_3 %d\n",readReg (0xc8));
  }
//}}}
