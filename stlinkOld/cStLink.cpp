//{{{  includes
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cStLink.h"
#include "libusb.h"

//}}}
//{{{  defines
#define USB_ST_VID          0x0483
#define USB_STLINK_32L_PID  0x3748
#define USB_STLINK_V21_PID  0x374B

//{{{  STLINK_DEBUG_RESETSYS, etc:
#define STLINK_OK             0x80
#define STLINK_FALSE          0x81

#define STLINK_CORE_RUNNING   0x80
#define STLINK_CORE_HALTED    0x81
#define STLINK_CORE_STAT_UNKNOWN  -1
//}}}

#define STLINK_DEBUG_ENTER_JTAG 0x00
#define STLINK_DEBUG_GETSTATUS    0x01
#define STLINK_DEBUG_FORCEDEBUG 0x02
#define STLINK_DEBUG_RESETSYS   0x03
#define STLINK_DEBUG_READALLREGS  0x04
#define STLINK_DEBUG_READREG    0x05
#define STLINK_DEBUG_WRITEREG   0x06
#define STLINK_DEBUG_READMEM_32BIT  0x07
#define STLINK_DEBUG_WRITEMEM_32BIT 0x08
#define STLINK_DEBUG_RUNCORE    0x09
#define STLINK_DEBUG_STEPCORE   0x0a
#define STLINK_DEBUG_SETFP    0x0b
#define STLINK_DEBUG_WRITEMEM_8BIT  0x0d
#define STLINK_DEBUG_CLEARFP    0x0e
#define STLINK_DEBUG_WRITEDEBUGREG  0x0f

#define STLINK_DEBUG_ENTER    0x20
#define STLINK_DEBUG_EXIT   0x21
#define STLINK_DEBUG_READCOREID 0x22

#define STLINK_SWD_ENTER 0x30
#define STLINK_SWD_READCOREID 0x32

#define STLINK_JTAG_WRITEDEBUG_32BIT 0x35
#define STLINK_JTAG_READDEBUG_32BIT 0x36
#define STLINK_JTAG_DRIVE_NRST 0x3c

#define STLINK_DEBUG_ENTER_SWD    0xa3
#define STLINK_DEBUG_COMMAND    0xF2

#define STLINK_DFU_EXIT   0x07
#define STLINK_DFU_COMMAND    0xF3

#define STLINK_GET_VERSION    0xf1
#define STLINK_GET_CURRENT_MODE 0xf5

//{{{  stm32f FPEC flash controller interface, pm0063 manual
// TODO - all of this needs to be abstracted out....
// STM32F05x is identical, based on RM0091 (DM00031936, Doc ID 018940 Rev 2, August 2012)
#define FLASH_REGS_ADDR 0x40022000
#define FLASH_REGS_SIZE 0x28

#define FLASH_ACR (FLASH_REGS_ADDR + 0x00)
#define FLASH_KEYR (FLASH_REGS_ADDR + 0x04)
#define FLASH_SR (FLASH_REGS_ADDR + 0x0c)
#define FLASH_CR (FLASH_REGS_ADDR + 0x10)
#define FLASH_AR (FLASH_REGS_ADDR + 0x14)
#define FLASH_OBR (FLASH_REGS_ADDR + 0x1c)
#define FLASH_WRPR (FLASH_REGS_ADDR + 0x20)
//}}}
//{{{  For STM32F05x, the RDPTR_KEY may be wrong, but as it is not used anywhere...
#define FLASH_RDPTR_KEY 0x00a5
#define FLASH_KEY1 0x45670123
#define FLASH_KEY2 0xcdef89ab

#define FLASH_SR_BSY 0
#define FLASH_SR_EOP 5

#define FLASH_CR_PG 0
#define FLASH_CR_PER 1
#define FLASH_CR_MER 2
#define FLASH_CR_STRT 6
#define FLASH_CR_LOCK 7
//}}}
//{{{  32L = 32F1 same CoreID as 32F4!
#define STM32L_FLASH_REGS_ADDR ((uint32_t)0x40023c00)
#define STM32L_FLASH_ACR (STM32L_FLASH_REGS_ADDR + 0x00)
#define STM32L_FLASH_PECR (STM32L_FLASH_REGS_ADDR + 0x04)
#define STM32L_FLASH_PDKEYR (STM32L_FLASH_REGS_ADDR + 0x08)
#define STM32L_FLASH_PEKEYR (STM32L_FLASH_REGS_ADDR + 0x0c)
#define STM32L_FLASH_PRGKEYR (STM32L_FLASH_REGS_ADDR + 0x10)
#define STM32L_FLASH_OPTKEYR (STM32L_FLASH_REGS_ADDR + 0x14)
#define STM32L_FLASH_SR (STM32L_FLASH_REGS_ADDR + 0x18)
#define STM32L_FLASH_OBR (STM32L_FLASH_REGS_ADDR + 0x1c)
#define STM32L_FLASH_WRPR (STM32L_FLASH_REGS_ADDR + 0x20)
#define FLASH_L1_FPRG 10
#define FLASH_L1_PROG 3
//}}}
//{{{  STM32F4
#define FLASH_F4_REGS_ADDR ((uint32_t)0x40023c00)
#define FLASH_F4_KEYR (FLASH_F4_REGS_ADDR + 0x04)
#define FLASH_F4_OPT_KEYR (FLASH_F4_REGS_ADDR + 0x08)
#define FLASH_F4_SR (FLASH_F4_REGS_ADDR + 0x0c)
#define FLASH_F4_CR (FLASH_F4_REGS_ADDR + 0x10)
#define FLASH_F4_OPT_CR (FLASH_F4_REGS_ADDR + 0x14)
#define FLASH_F4_CR_STRT 16
#define FLASH_F4_CR_LOCK 31
#define FLASH_F4_CR_SER 1
#define FLASH_F4_CR_SNB 3
#define FLASH_F4_CR_SNB_MASK 0x38
#define FLASH_F4_SR_BSY 16
//}}}
//{{{  cortex core ids
// TODO clean this up...
#define STM32VL_CORE_ID 0x1ba01477
#define STM32L_CORE_ID 0x2ba01477
#define STM32F3_CORE_ID 0x2ba01477
#define STM32F4_CORE_ID 0x2ba01477
#define STM32F0_CORE_ID 0xbb11477

#define CORE_M3_R1 0x1BA00477
#define CORE_M3_R2 0x4BA00477
#define CORE_M4_R0 0x2BA01477
//}}}
//{{{  Chip IDs are explained in the appropriate programming manual for the * DBGMCU_IDCODE register (0xE0042000)
// stm32 chipids, only lower 12 bits..
#define STM32_CHIPID_F1_MEDIUM 0x410
#define STM32_CHIPID_F2 0x411
#define STM32_CHIPID_F1_LOW 0x412
#define STM32_CHIPID_F3 0x422
#define STM32_CHIPID_F37x 0x432
#define STM32_CHIPID_F4 0x413
#define STM32_CHIPID_F1_HIGH 0x414
#define STM32_CHIPID_L1_MEDIUM 0x416
#define STM32_CHIPID_F1_CONN 0x418
#define STM32_CHIPID_F1_VL_MEDIUM 0x420
#define STM32_CHIPID_F1_VL_HIGH 0x428
#define STM32_CHIPID_F1_XL 0x430
#define STM32_CHIPID_F0 0x440
//}}}
//{{{  Constant STM32 memory map figures
#define STM32_FLASH_BASE 0x08000000
#define STM32_SRAM_BASE  0x20000000
//}}}
//{{{  Cortex™-M3 Technical Reference Manual
/* Debug Halting Control and Status Register */
#define DHCSR 0xe000edf0
#define DCRSR 0xe000edf4
#define DCRDR 0xe000edf8
#define DBGKEY 0xa05f0000
//}}}
//}}}

//{{{

struct trans_ctx {
  #define TRANS_FLAGS_IS_DONE (1 << 0)
  #define TRANS_FLAGS_HAS_ERROR (1 << 1)
    volatile unsigned long flags;
  };
//}}}
//{{{
static void on_trans_done (struct libusb_transfer* trans) {

  struct trans_ctx* const ctx = (struct trans_ctx*)(trans->user_data);
  if (trans->status != LIBUSB_TRANSFER_COMPLETED)
    ctx->flags |= TRANS_FLAGS_HAS_ERROR;
  ctx->flags |= TRANS_FLAGS_IS_DONE;
  }
//}}}

cStLink::cStLink() {}
//{{{
cStLink::~cStLink() {

  if (req_trans != NULL)
    libusb_free_transfer (req_trans);
  if (rep_trans != NULL)
    libusb_free_transfer (rep_trans);
  if (usb_handle != NULL)
    libusb_close (usb_handle);
  libusb_exit (libusb_ctx);
  }
//}}}

//{{{
int cStLink::openUsb() {

  core_stat = STLINK_CORE_STAT_UNKNOWN;

  if (libusb_init (&(libusb_ctx))) {
    printf ("failed to init libusb context\n");
    return 0;
    }

  usb_handle = libusb_open_device_with_vid_pid (libusb_ctx, USB_ST_VID, USB_STLINK_V21_PID);
  if (usb_handle == NULL) {
    usb_handle = libusb_open_device_with_vid_pid (libusb_ctx, USB_ST_VID, USB_STLINK_32L_PID);
    if (usb_handle == NULL) {
      printf ("Couldn't find any ST-Link/V2 devices\n");
      return 0;
      }
    else
      printf ("st link v2 found\n");
    }
  else
    printf ("st link v21 found\n");

  if (libusb_kernel_driver_active (usb_handle, 0) == 1) {
    int r = libusb_detach_kernel_driver (usb_handle, 0);
    if (r < 0) {
      printf  ("libusb_detach_kernel_driver(() error %s\n", strerror(-r));
      return 0;
      }
    }

  int config;
  if (libusb_get_configuration (usb_handle, &config)) {
    /* this may fail for a previous configured device */
    printf ("libusb_get_configuration()\n");
    return 0;
    }

  if (config != 1) {
    printf ("setting new configuration (%d -> 1)\n", config);
    if (libusb_set_configuration(usb_handle, 1)) {
      /* this may fail for a previous configured device */
      printf ("libusb_set_configuration() failed\n");
      return 0;
      }
    }

  if (libusb_claim_interface (usb_handle, 0)) {
    printf ("libusb_claim_interface() failed\n");
    return 0;
    }

  req_trans = libusb_alloc_transfer(0);
  if (req_trans == NULL) {
    printf ("libusb_alloc_transfer failed\n");
    return 0;
    }

  rep_trans = libusb_alloc_transfer(0);
  if (rep_trans == NULL) {
    printf ("libusb_alloc_transfer failed\n");
    return 0;
    }

  ep_rep = 1 | LIBUSB_ENDPOINT_IN;
  ep_req = 2 | LIBUSB_ENDPOINT_OUT;
  mCmdLen = 16;

  if (getCurrentMode(0) == STLINK_DEV_DFU_MODE) {
    printf ("-- exit_dfu_mode\n");
    exitDfuMode();
    }

  if (getCurrentMode(0) != STLINK_DEV_DEBUG_MODE)
    enterSwdMode();

  reset();
  load_device_params();
  getVersion();
  return 1;
  }
//}}}

//{{{
void cStLink::getVersion() {

  mCmdBuf[0] = STLINK_GET_VERSION;
  ssize_t size = sendRecv (mCmdLen, 6);
  if (size == -1) {
    printf("[!] send_recv\n");
    }

  uint32_t b0 = mDataBuf[0]; //lsb
  uint32_t b1 = mDataBuf[1];
  uint32_t b2 = mDataBuf[2];
  uint32_t b3 = mDataBuf[3];
  uint32_t b4 = mDataBuf[4];
  uint32_t b5 = mDataBuf[5]; //msb

  // b0 b1                       || b2 b3  | b4 b5
  // 4b        | 6b     | 6b     || 2B     | 2B
  // stlink_v  | jtag_v | swim_v || st_vid | stlink_pid
  version.stlink_v =   (b0 & 0xf0) >> 4;
  version.jtag_v =    ((b0 & 0x0f) << 2) | ((b1 & 0xc0) >> 6);
  version.swim_v =      b1 & 0x3f;
  version.st_vid =     (b3 << 8) | b2;
  version.stlink_pid = (b5 << 8) | b4;

  printf ("StLink vid:pid 0x%04x:0x%04x version:%d jtag version:%d\n",
          version.st_vid, version.stlink_pid, version.stlink_v, version.jtag_v);
  }
//}}}
//{{{
uint32_t cStLink::getCoreId() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_READCOREID;
  ssize_t size = sendRecv (mCmdLen, 4);
  if (size == -1) {
    printf("[!] send_recv\n");
    return 0;
    }

  memcpy (&core_id, mDataBuf, 4);
  return core_id;
  }
//}}}
//{{{
uint32_t cStLink::getChipId() {

  uint32_t chip_id = readDebug32 (0xE0042000);
  if (chip_id == 0)
    // Try Corex M0 DBGMCU_IDCODE register address
    chip_id = readDebug32(0x40015800);
  return chip_id;
  }
//}}}
//{{{
void cStLink::getCpuId (cortex_m3_cpuid_t *cpuid) {

  uint32_t raw = readDebug32 (CM3_REG_CPUID);

  cpuid->implementer_id = (raw >> 24) & 0x7f;
  cpuid->variant = (raw >> 20) & 0xf;
  cpuid->part = (raw >> 4) & 0xfff;
  cpuid->revision = raw & 0xf;
  return;
  }
//}}}
//{{{
void cStLink::getStatus() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_GETSTATUS;

  ssize_t size = sendRecv (mCmdLen, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }

  if (mDataLen <= 0)
     return;

  switch (mDataBuf[0]) {
    case STLINK_CORE_RUNNING:
      core_stat = STLINK_CORE_RUNNING;
      printf ("status: running\n");
      return;
    case STLINK_CORE_HALTED:
      core_stat = STLINK_CORE_HALTED;
      printf ("status: halted\n");
      return;
    default:
      core_stat = STLINK_CORE_STAT_UNKNOWN;
      printf ("status: unknown\n");
    }
  }
//}}}
//{{{
int cStLink::getCurrentMode (int report) {

  mCmdBuf[0] = STLINK_GET_CURRENT_MODE;
  ssize_t size = sendRecv (mCmdLen, 2);
  if (size == -1) {
      printf("[!] send_recv\n");
      return -1;
    }
  int mode = mDataBuf[0];

  switch (mode) {
    case STLINK_DEV_DFU_MODE:
      if (report)
        printf ("stlink current mode: dfu\n");
      break;
    case STLINK_DEV_DEBUG_MODE:
      if (report)
        printf ("stlink current mode: debug (jtag or swd)\n");
      break;
    case STLINK_DEV_MASS_MODE:
      if (report)
        printf ("stlink current mode: dfu\n");
      break;
    default:
      printf ("stlink mode: unknown!\n");
      mode = STLINK_DEV_UNKNOWN_MODE;
      break;
      }

  return mode;
  }
//}}}

//{{{
void cStLink::enterSwdMode() {

  unsigned char* const cmd  = mCmdBuf;
  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_ENTER;
  mCmdBuf[2] = STLINK_DEBUG_ENTER_SWD;
  ssize_t size = sendOnly (mCmdLen);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::exitDebugMode() {

  writeDebug32 (DHCSR, DBGKEY);

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_EXIT;
  ssize_t size = sendOnly (mCmdLen);
  if (size == -1) {
    printf("[!] send_only\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::exitDfuMode() {

  mCmdBuf[0] = STLINK_DFU_COMMAND;
  mCmdBuf[1] = STLINK_DFU_EXIT;
  ssize_t size = sendOnly(mCmdLen);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}

//{{{
uint32_t cStLink::readDebug32 (uint32_t addr) {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_JTAG_READDEBUG_32BIT;
  memcpy (&mCmdBuf[2], &addr, 4);
  ssize_t size = sendRecv (mCmdLen, 8);
  if (size == -1) {
    printf("[!] send_recv\n");
    return 0;
    }

  uint32_t data;
  memcpy (&data, mDataBuf, 4);
  return data;
  }
//}}}
//{{{
void cStLink::readMem32 (uint32_t addr, uint16_t len) {

  if (len % 4 != 0) {
    // !!! never ever: fw gives just wrong values
    printf ("Error: Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
    abort();
    }

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_READMEM_32BIT;
  memcpy (&mCmdBuf[2], &addr, 4);
  memcpy (&mCmdBuf[6], &len, 2);
  ssize_t size = sendRecv (mCmdLen, len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  mDataLen = (size_t) size;
  }
//}}}
//{{{
void cStLink::readReg (int r_idx, reg *regp) {

  if (r_idx > 20 || r_idx < 0) {
    printf ("Error: register index must be in [0..20]\n");
    return;
    }

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_READREG;
  mCmdBuf[2] = (uint8_t) r_idx;
  ssize_t size = sendRecv (mCmdLen, 4);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  mDataLen = (size_t) size;

  uint32_t r;
  memcpy (&r, mDataBuf, 4);
  switch (r_idx) {
    case 16:
      regp->xpsr = r;
      break;
    case 17:
      regp->main_sp = r;
      break;
    case 18:
      regp->process_sp = r;
      break;
    case 19:
      regp->rw = r; /* XXX ?(primask, basemask etc.) */
      break;
    case 20:
      regp->rw2 = r; /* XXX ?(primask, basemask etc.) */
      break;
    default:
      regp->r[r_idx] = r;
    }
  }
//}}}
//{{{
void cStLink::readAllRegs (reg *regp) {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_READALLREGS;
  ssize_t size = sendRecv (mCmdLen, 84);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  mDataLen = (size_t) size;

  for (int i = 0; i < 16; i++)
    memcpy (&regp->r[i], mDataBuf + i*4, 4);
  memcpy (&regp->xpsr, mDataBuf + 64, 4);
  memcpy (&regp->main_sp, mDataBuf + 68, 4);
  memcpy (&regp->process_sp, mDataBuf + 72, 4);
  memcpy (&regp->rw, mDataBuf + 76, 4);
  memcpy (&regp->rw2, mDataBuf + 80, 4);

  printf ("xpsr       = 0x%08x\n", regp->xpsr);
  printf ("main_sp    = 0x%08x\n", regp->main_sp);
  printf ("process_sp = 0x%08x\n", regp->process_sp);
  printf ("rw         = 0x%08x\n", regp->rw);
  printf ("rw2        = 0x%08x\n", regp->rw2);
  }
//}}}
//{{{
void cStLink::readUnsupportedReg (int r_idx, reg *regp) {

  int r_convert;

  /* Convert to values used by DCRSR */
  if (r_idx >= 0x1C && r_idx <= 0x1F) { /* primask, basepri, faultmask, or control */
    r_convert = 0x14;
    }
  else if (r_idx == 0x40) {     /* FPSCR */
    r_convert = 0x21;
    }
  else if (r_idx >= 0x20 && r_idx < 0x40) {
    r_convert = 0x40 + (r_idx - 0x20);
    }
  else {
    printf ("Error: register address must be in [0x1C..0x40]\n");
    return;
    }

  uint32_t r;

  mDataBuf[0] = (unsigned char) r_idx;
  for (int i = 1; i < 4; i++) {
    mDataBuf[i] = 0;
    }

  writeMem32 (DCRSR, 4);
  readMem32 (DCRDR, 4);
  memcpy (&r, mDataBuf, 4);

  switch (r_idx) {
    case 0x14:
      regp->primask = (uint8_t) (r & 0xFF);
      regp->basepri = (uint8_t) ((r>>8) & 0xFF);
      regp->faultmask = (uint8_t) ((r>>16) & 0xFF);
      regp->control = (uint8_t) ((r>>24) & 0xFF);
      break;
    case 0x21:
      regp->fpscr = r;
      break;
    default:
      regp->s[r_idx - 0x40] = r;
      break;
    }
  }
//}}}
//{{{
void cStLink::readAllUnsupportedRegs (reg *regp) {

  readUnsupportedReg (0x14, regp);
  readUnsupportedReg (0x21, regp);
  for (int i = 0; i < 32; i++)
    readUnsupportedReg (0x40+i, regp);
  }
//}}}

//{{{
void cStLink::writeDebug32 (uint32_t addr, uint32_t data) {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_JTAG_WRITEDEBUG_32BIT;
  memcpy (&mCmdBuf[2], &addr, 4);
  memcpy (&mCmdBuf[6], &data, 4);
  ssize_t size = sendRecv (mCmdLen, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::writeMem32 (uint32_t addr, uint16_t len) {
  if (len % 4 != 0) {
    fprintf(stderr, "Error: Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
    abort();
    }

  int i = 0;
  mCmdBuf[i++] = STLINK_DEBUG_COMMAND;
  mCmdBuf[i++] = STLINK_DEBUG_WRITEMEM_32BIT;
  memcpy (&mCmdBuf[i], &addr, 4);
  memcpy (&mCmdBuf[i+4], &len, 2);
  sendOnly (mCmdLen);
  sendOnlyData (len);
  }
//}}}
//{{{
void cStLink::writeMem8 (uint32_t addr, uint16_t len) {

  if (len > 0x40 ) { // !!! never ever: Writing more then 0x40 bytes gives unexpected behaviour
    printf ("Error: Data length > 64: +%d byte.\n", len);
    abort();
    }

  int i = 0;
  mCmdBuf[i++] = STLINK_DEBUG_COMMAND;
  mCmdBuf[i++] = STLINK_DEBUG_WRITEMEM_8BIT;
  memcpy (&mCmdBuf[i], &addr, 4);
  memcpy (&mCmdBuf[i+4], &len, 2);
  sendOnly (mCmdLen);
  sendOnly (len);

  }
//}}}
//{{{
void cStLink::writeReg (uint32_t reg, int idx) {

    mCmdBuf[0] = STLINK_DEBUG_COMMAND;
    mCmdBuf[1] = STLINK_DEBUG_WRITEREG;
    mCmdBuf[2] = idx;
    memcpy (&mCmdBuf[3], &reg, 4);
    ssize_t size = sendRecv (mCmdLen, 2);
    if (size == -1) {
      printf("[!] send_recv\n");
      return;
      }
    mDataLen = (size_t) size;
  }
//}}}
//{{{
void cStLink::writeUnsupportedReg (uint32_t val, int r_idx, reg *regp) {
  int r_convert;

  /* Convert to values used by DCRSR */
  if (r_idx >= 0x1C && r_idx <= 0x1F) { /* primask, basepri, faultmask, or control */
    r_convert = r_idx;  /* The backend function handles this */
  } else if (r_idx == 0x40) {     /* FPSCR */
    r_convert = 0x21;
  } else if (r_idx >= 0x20 && r_idx < 0x40) {
    r_convert = 0x40 + (r_idx - 0x20);
  } else {
    printf ("Error: register address must be in [0x1C..0x40]\n");
    return;
    }

  if (r_idx >= 0x1C && r_idx <= 0x1F) {
    /* primask, basepri, faultmask, or control These are held in the same register */
    readUnsupportedReg (0x14, regp);
    val = (uint8_t) (val>>24);
    switch (r_idx) {
      case 0x1C:  /* control */
        val = (((uint32_t) val) << 24) | (((uint32_t) regp->faultmask) << 16) | (((uint32_t) regp->basepri) << 8) | ((uint32_t) regp->primask);
        break;
      case 0x1D:  /* faultmask */
        val = (((uint32_t) regp->control) << 24) | (((uint32_t) val) << 16) | (((uint32_t) regp->basepri) << 8) | ((uint32_t) regp->primask);
        break;
      case 0x1E:  /* basepri */
        val = (((uint32_t) regp->control) << 24) | (((uint32_t) regp->faultmask) << 16) | (((uint32_t) val) << 8) | ((uint32_t) regp->primask);
        break;
      case 0x1F:  /* primask */
        val = (((uint32_t) regp->control) << 24) | (((uint32_t) regp->faultmask) << 16) | (((uint32_t) regp->basepri) << 8) | ((uint32_t) val);
        break;
      }

    r_idx = 0x14;
    }

  memcpy (mDataBuf, &val, 4);
  writeMem32 (DCRDR, 4);

  mDataBuf[0] = (unsigned char) r_idx;
  mDataBuf[1] = 0;
  mDataBuf[2] = 0x01;
  mDataBuf[3] = 0;
  writeMem32 (DCRSR, 4);
  }
//}}}

//{{{
void cStLink::reset() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_RESETSYS;
  ssize_t size = sendRecv (mCmdLen, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::jtagReset (int value) {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_JTAG_DRIVE_NRST;
  mCmdBuf[2] = (value) ? 0 : 1;
  ssize_t size = sendRecv (mCmdLen, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
// Force the core into the debug mode -> halted state.
void cStLink::forceDebug() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_FORCEDEBUG;
  ssize_t size = sendRecv (mCmdLen, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::step() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_STEPCORE;
  ssize_t size = sendRecv (mCmdLen, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::run() {

  mCmdBuf[0] = STLINK_DEBUG_COMMAND;
  mCmdBuf[1] = STLINK_DEBUG_RUNCORE;
  ssize_t size = sendRecv (mCmdLen, 2);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::runAt (stm32_addr_t addr) {

  writeReg (addr, 15); /* pc register */
  run();
  while (is_core_halted() == 0)
        Sleep(3000000);
  }
//}}}

// private
//{{{
int cStLink::submit_wait (struct libusb_transfer* trans) {

  struct trans_ctx trans_ctx;

  trans_ctx.flags = 0;

  /* brief intrusion inside the libusb interface */
  trans->callback = on_trans_done;
  trans->user_data = &trans_ctx;

  int error = libusb_submit_transfer (trans);
  if (error) {
    printf ("libusb_submit_transfer(%d)\n", error);
    return -1;
    }

  while (trans_ctx.flags == 0) {
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (libusb_handle_events_timeout (libusb_ctx, &timeout)) {
      printf ("libusb_handle_events()\n");
      return -1;
      }
    }

  if (trans_ctx.flags & TRANS_FLAGS_HAS_ERROR) {
    printf ("libusb_handle_events() | has_error\n");
    return -1;
    }

  return 0;
  }
//}}}
//{{{
int cStLink::sendRecv (int txsize, int rxsize) {

  /* note: txbuf and rxbuf can point to the same area */
  int res = 0;
  libusb_fill_bulk_transfer (req_trans, usb_handle, ep_req, mCmdBuf, txsize, NULL, NULL, 0);
  if (submit_wait (req_trans))
    return -1;

  /* send_only */
  if (rxsize != 0) {
    /* read the response */
    libusb_fill_bulk_transfer (rep_trans, usb_handle, ep_rep, mDataBuf, rxsize, NULL, NULL, 0);
    if (submit_wait (rep_trans))
      return -1;
    res = rep_trans->actual_length;
    }

  return rep_trans->actual_length;
  }
//}}}
//{{{
int cStLink::sendOnly (int txsize) {
  libusb_fill_bulk_transfer (req_trans, usb_handle, ep_req, mCmdBuf, txsize, NULL, NULL, 0);
  if (submit_wait (req_trans))
    return -1;
  return rep_trans->actual_length;
  }
//}}}
//{{{
int cStLink::sendOnlyData (int txsize) {
  libusb_fill_bulk_transfer (req_trans, usb_handle, ep_req, mDataBuf, txsize, NULL, NULL, 0);
  if (submit_wait (req_trans))
    return -1;
  return rep_trans->actual_length;
  }
//}}}
//{{{
int cStLink::load_device_params() {

  printf ("Loading device parameters....\n");
  const chip_params_t *params = NULL;
  core_id = getCoreId();
  uint32_t chip_id = getChipId();

  chip_id = chip_id & 0xfff;
  /* Fix chip_id for F4 rev A errata , Read CPU ID, as CoreID is the same for F2/F4*/
  if (chip_id == 0x411) {
    uint32_t cpuid = readDebug32(0xE000ED00);
    if ((cpuid  & 0xfff0) == 0xc240)
        chip_id = 0x413;
    }

  for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
    if(devices[i].chip_id == chip_id) {
      params = &devices[i];
      break;
      }
    }
  if (params == NULL) {
    printf ("unknown chip id! %#x\n", chip_id);
    return -1;
    }

  // These are fixed...
  flash_base = STM32_FLASH_BASE;
  sram_base = STM32_SRAM_BASE;

  // read flash size from hardware, if possible...
  if (chip_id == STM32_CHIPID_F2) {
    flash_size = 0x100000; /* Use maximum, User must care!*/
    }
  else if (chip_id == STM32_CHIPID_F4) {
    flash_size = 0x100000;      //todo: RM0090 error; size register same address as unique ID
    }
  else {
    uint32_t flash_size = readDebug32(params->flash_size_reg) & 0xffff;
    flash_size = flash_size * 1024;
    }

  flash_pgsz = params->flash_pagesize;
  sram_size = params->sram_size;
  sys_base = params->bootrom_base;
  sys_size = params->bootrom_size;

  printf ("Device connected is: %s, id %#x\n", params->description, chip_id);
  // TODO make note of variable page size here.....
  printf ("SRAM size: %#x bytes (%d KiB), Flash: %#x bytes (%d KiB) in pages of %zd bytes\n",
          sram_size, sram_size / 1024, flash_size, flash_size / 1024,
          flash_pgsz);
  return 0;
}
//}}}
//{{{
unsigned int cStLink::is_core_halted() {

  /* return non zero if core is halted */
  getStatus();
  return mDataBuf[0] == STLINK_CORE_HALTED;
  }
//}}}
