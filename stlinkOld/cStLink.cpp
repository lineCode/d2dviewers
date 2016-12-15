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
#define STLINK_CMD_SIZE 16

//{{{
inline unsigned int is_bigendian() {
  static volatile const unsigned int i = 1;
  return *(volatile const char*) &i == 0;
  }
//}}}
//{{{
void write_uint32 (unsigned char* buf, uint32_t ui) {
    if (!is_bigendian()) { // le -> le (don't swap)
        buf[0] = ((unsigned char*) &ui)[0];
        buf[1] = ((unsigned char*) &ui)[1];
        buf[2] = ((unsigned char*) &ui)[2];
        buf[3] = ((unsigned char*) &ui)[3];
    } else {
        buf[0] = ((unsigned char*) &ui)[3];
        buf[1] = ((unsigned char*) &ui)[2];
        buf[2] = ((unsigned char*) &ui)[1];
        buf[3] = ((unsigned char*) &ui)[0];
    }
}
//}}}
//{{{
void write_uint16 (unsigned char* buf, uint16_t ui) {
    if (!is_bigendian()) { // le -> le (don't swap)
        buf[0] = ((unsigned char*) &ui)[0];
        buf[1] = ((unsigned char*) &ui)[1];
    } else {
        buf[0] = ((unsigned char*) &ui)[1];
        buf[1] = ((unsigned char*) &ui)[0];
    }
}
//}}}
//{{{
uint32_t read_uint32 (const unsigned char *c, const int pt) {
    uint32_t ui;
    char *p = (char *) &ui;

    if (!is_bigendian()) { // le -> le (don't swap)
        p[0] = c[pt + 0];
        p[1] = c[pt + 1];
        p[2] = c[pt + 2];
        p[3] = c[pt + 3];
    } else {
        p[0] = c[pt + 3];
        p[1] = c[pt + 2];
        p[2] = c[pt + 1];
        p[3] = c[pt + 0];
    }
    return ui;
}
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
int cStLink::open_usb() {

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
  cmd_len = STLINK_CMD_SIZE;

  if (current_mode (0) == STLINK_DEV_DFU_MODE) {
    printf ("-- exit_dfu_mode\n");
    exit_dfu_mode();
    }

  if (current_mode (0) != STLINK_DEV_DEBUG_MODE)
    enter_swd_mode();

  reset();
  load_device_params();
  getVersion();
  }
//}}}
//{{{
void cStLink::getVersion() {

  unsigned char* const data = q_buf;
  unsigned char* const cmd  = c_buf;
  ssize_t size;
  uint32_t rep_len = 6;
  int i = 0;
  cmd[i++] = STLINK_GET_VERSION;

  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    }

  uint32_t b0 = q_buf[0]; //lsb
  uint32_t b1 = q_buf[1];
  uint32_t b2 = q_buf[2];
  uint32_t b3 = q_buf[3];
  uint32_t b4 = q_buf[4];
  uint32_t b5 = q_buf[5]; //msb

  // b0 b1                       || b2 b3  | b4 b5
  // 4b        | 6b     | 6b     || 2B     | 2B
  // stlink_v  | jtag_v | swim_v || st_vid | stlink_pid
  version.stlink_v = (b0 & 0xf0) >> 4;
  version.jtag_v = ((b0 & 0x0f) << 2) | ((b1 & 0xc0) >> 6);
  version.swim_v = b1 & 0x3f;
  version.st_vid = (b3 << 8) | b2;
  version.stlink_pid = (b5 << 8) | b4;

  printf ("stLink vid:pid 0x%04x:0x%04x stLink version:%5d jtag version:%d\n",
          version.st_vid, version.stlink_pid, version.stlink_v, version.jtag_v);
  }
//}}}

//{{{
uint32_t cStLink::get_core_id() {

  unsigned char* const cmd  = c_buf;
  unsigned char* const data = q_buf;
  ssize_t size;
  int rep_len = 4;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_READCOREID;

  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return 0;
    }

  core_id = read_uint32(data, 0);

  printf ("core_id = 0x%08x\n", core_id);

  return core_id;
  }
//}}}
//{{{
uint32_t cStLink::get_chip_id() {
  uint32_t chip_id = read_debug32 (0xE0042000);
  if (chip_id == 0)
    // Try Corex M0 DBGMCU_IDCODE register address
    chip_id = read_debug32(0x40015800);
  return chip_id;
  }
//}}}
//{{{
void cStLink::get_cpu_id (cortex_m3_cpuid_t *cpuid) {
  uint32_t raw = read_debug32 (CM3_REG_CPUID);
  cpuid->implementer_id = (raw >> 24) & 0x7f;
  cpuid->variant = (raw >> 20) & 0xf;
  cpuid->part = (raw >> 4) & 0xfff;
  cpuid->revision = raw & 0xf;
  return;
}
  //}}}

//{{{
void cStLink::reset() {

  unsigned char* const data = q_buf;
  unsigned char* const cmd = c_buf;
  ssize_t size;
  int rep_len = 2;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_RESETSYS;

  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::jtag_reset (int value) {

  unsigned char* const data = q_buf;
  unsigned char* const cmd = c_buf;
  ssize_t size;
  int rep_len = 2;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_JTAG_DRIVE_NRST;
  cmd[i++] = (value)?0:1;

  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}

//{{{
int cStLink::current_mode (int report) {

  unsigned char* const cmd  = c_buf;
  unsigned char* const data = q_buf;
  ssize_t size;
  int rep_len = 2;
  int i = 0;
  cmd[i++] = STLINK_GET_CURRENT_MODE;
  size = sendRecv (1, cmd,  cmd_len, data, rep_len);
  if (size == -1) {
      printf("[!] send_recv\n");
      return -1;
    }
  int mode = q_buf[0];

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
void cStLink::enter_swd_mode() {

  unsigned char* const cmd  = c_buf;
  ssize_t size;
  const int rep_len = 0;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_ENTER;
  cmd[i++] = STLINK_DEBUG_ENTER_SWD;
  size = sendOnly (1, cmd, cmd_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
// Force the core into the debug mode -> halted state.
void cStLink::force_debug() {

  unsigned char* const data = q_buf;
  unsigned char* const cmd  = c_buf;
  ssize_t size;
  int rep_len = 2;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_FORCEDEBUG;
  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::exit_debug_mode() {

  write_debug32 (DHCSR, DBGKEY);

  unsigned char* const cmd = c_buf;
  ssize_t size;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_EXIT;

  size = sendOnly (1, cmd, cmd_len);
  if (size == -1) {
    printf("[!] send_only\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::exit_dfu_mode() {

  unsigned char* const cmd = c_buf;
  ssize_t size;
  int i = 0;
  cmd[i++] = STLINK_DFU_COMMAND;
  cmd[i++] = STLINK_DFU_EXIT;

  size = sendOnly(1, cmd, cmd_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}

//{{{
void cStLink::getStatus() {

  unsigned char* const data = q_buf;
  unsigned char* const cmd  = c_buf;
  ssize_t size;
  int rep_len = 2;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_GETSTATUS;

  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }

  if (q_len <= 0)
     return;

  switch (q_buf[0]) {
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
uint32_t cStLink::read_debug32 (uint32_t addr) {

  unsigned char* const rdata = q_buf;
  unsigned char* const cmd  = c_buf;
  ssize_t size;
  const int rep_len = 8;

  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_JTAG_READDEBUG_32BIT;
  write_uint32(&cmd[i], addr);
  size = sendRecv (1, cmd, cmd_len, rdata, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return 0;
    }

  uint32_t data = read_uint32 (rdata, 4);
  //printf ("*** stlink_read_debug32 %x is %#x\n", data, addr);
  return data;
  }
//}}}
//{{{
void cStLink::read_mem32 (uint32_t addr, uint16_t len) {

  if (len % 4 != 0) {
    // !!! never ever: fw gives just wrong values
    printf ("Error: Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
    abort();
    }

  unsigned char* const data = q_buf;
  unsigned char* const cmd = c_buf;
  ssize_t size;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_READMEM_32BIT;
  write_uint32 (&cmd[i], addr);
  write_uint16 (&cmd[i + 4], len);

  size = sendRecv (1, cmd, cmd_len, data, len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }

  q_len = (size_t) size;

  print_data();
  }
//}}}

//{{{
void cStLink::write_debug32 (uint32_t addr, uint32_t data) {

  printf ("*** write_debug32 %x to %#x\n", data, addr);
  unsigned char* const rdata = q_buf;
  unsigned char* const cmd  = c_buf;
  ssize_t size;
  const int rep_len = 2;

  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_JTAG_WRITEDEBUG_32BIT;
  write_uint32 (&cmd[i], addr);
  write_uint32 (&cmd[i + 4], data);
  size = sendRecv (1, cmd, cmd_len, rdata, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::write_mem32 (uint32_t addr, uint16_t len) {
    printf ("*** stlink_write_mem32 %u bytes to %#x\n", len, addr);
    if (len % 4 != 0) {
        fprintf(stderr, "Error: Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
        abort();
        }

    unsigned char* const data = q_buf;
    unsigned char* const cmd  = c_buf;

    int i = 0;
    cmd[i++] = STLINK_DEBUG_COMMAND;
    cmd[i++] = STLINK_DEBUG_WRITEMEM_32BIT;
    write_uint32(&cmd[i], addr);
    write_uint16(&cmd[i + 4], len);
    sendOnly (0, cmd, cmd_len);
    sendOnly (1, data, len);
  }
//}}}
//{{{
void cStLink::write_mem8 (uint32_t addr, uint16_t len) {

  printf ("*** stlink_write_mem8 ***\n");
  if (len > 0x40 ) { // !!! never ever: Writing more then 0x40 bytes gives unexpected behaviour
    printf ("Error: Data length > 64: +%d byte.\n", len);
    abort();
    }

  unsigned char* const data = q_buf;
  unsigned char* const cmd  = c_buf;

  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_WRITEMEM_8BIT;
  write_uint32 (&cmd[i], addr);
  write_uint16 (&cmd[i + 4], len);
  sendOnly (0, cmd, cmd_len);
  sendOnly (1, data, len);
  }
//}}}

//{{{
void cStLink::read_reg (int r_idx, reg *regp) {

  printf (" (%d) ***\n", r_idx);
  if (r_idx > 20 || r_idx < 0) {
    printf ("Error: register index must be in [0..20]\n");
    return;
    }

  unsigned char* const data = q_buf;
  unsigned char* const cmd  = c_buf;
  ssize_t size;
  uint32_t r;
  uint32_t rep_len = 4;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_READREG;
  cmd[i++] = (uint8_t) r_idx;
  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }

  q_len = (size_t) size;
  print_data();
  r = read_uint32(q_buf, 0);
  printf ("r_idx (%2d) = 0x%08x\n", r_idx, r);

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
void cStLink::read_all_regs (reg *regp) {

  printf ("*** read_all_regs ***\n");

  unsigned char* const cmd = c_buf;
  unsigned char* const data = q_buf;
  ssize_t size;
  uint32_t rep_len = 84;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_READALLREGS;
  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  q_len = (size_t) size;
  print_data();

  for (i = 0; i < 16; i++)
      regp->r[i]= read_uint32(q_buf, i*4);
  regp->xpsr       = read_uint32(q_buf, 64);
  regp->main_sp    = read_uint32(q_buf, 68);
  regp->process_sp = read_uint32(q_buf, 72);
  regp->rw         = read_uint32(q_buf, 76);
  regp->rw2        = read_uint32(q_buf, 80);

  printf ("xpsr       = 0x%08x\n", read_uint32(q_buf, 64));
  printf ("main_sp    = 0x%08x\n", read_uint32(q_buf, 68));
  printf ("process_sp = 0x%08x\n", read_uint32(q_buf, 72));
  printf ("rw         = 0x%08x\n", read_uint32(q_buf, 76));
  printf ("rw2        = 0x%08x\n", read_uint32(q_buf, 80));
  }
//}}}
//{{{
void cStLink::read_unsupported_reg (int r_idx, reg *regp) {

  int r_convert;
  printf ("*** read_unsupported_reg\n");
  printf (" (%d) ***\n", r_idx);

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

  q_buf[0] = (unsigned char) r_idx;
  for (int i = 1; i < 4; i++) {
    q_buf[i] = 0;
    }

  write_mem32 (DCRSR, 4);
  read_mem32 (DCRDR, 4);

  r = read_uint32 (q_buf, 0);
  printf("r_idx (%2d) = 0x%08x\n", r_idx, r);

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
void cStLink::read_all_unsupported_regs (reg *regp) {

  printf ("*** read_all_unsupported_regs ***\n");

  read_unsupported_reg (0x14, regp);
  read_unsupported_reg (0x21, regp);
  for (int i = 0; i < 32; i++)
    read_unsupported_reg (0x40+i, regp);
  }
//}}}
//{{{
void cStLink::write_reg (uint32_t reg, int idx) {

    printf ("*** write_reg\n");
    unsigned char* const data = q_buf;
    unsigned char* const cmd  = c_buf;
    ssize_t size;
    uint32_t rep_len = 2;
    int i = 0;
    cmd[i++] = STLINK_DEBUG_COMMAND;
    cmd[i++] = STLINK_DEBUG_WRITEREG;
    cmd[i++] = idx;
    write_uint32(&cmd[i], reg);
    size = sendRecv (1, cmd, cmd_len, data, rep_len);
    if (size == -1) {
      printf("[!] send_recv\n");
      return;
      }
    q_len = (size_t) size;
    print_data();
}
//}}}
//{{{
void cStLink::write_unsupported_reg (uint32_t val, int r_idx, reg *regp) {
  int r_convert;

  printf ("*** write_unsupported_reg\n");
  printf (" (%d) ***\n", r_idx);

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
    read_unsupported_reg (0x14, regp);
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

  write_uint32 (q_buf, val);
  write_mem32(DCRDR, 4);

  q_buf[0] = (unsigned char) r_idx;
  q_buf[1] = 0;
  q_buf[2] = 0x01;
  q_buf[3] = 0;

  write_mem32 (DCRSR, 4);
  }
//}}}

//{{{
void cStLink::step() {

  printf ("*** step ***\n");

  unsigned char* const data = q_buf;
  unsigned char* const cmd = c_buf;
  ssize_t size;
  int rep_len = 2;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_STEPCORE;

  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::run() {
  printf ("*** run ***\n");

  unsigned char* const data = q_buf;
  unsigned char* const cmd = c_buf;
  ssize_t size;
  int rep_len = 2;
  int i = 0;
  cmd[i++] = STLINK_DEBUG_COMMAND;
  cmd[i++] = STLINK_DEBUG_RUNCORE;

  size = sendRecv (1, cmd, cmd_len, data, rep_len);
  if (size == -1) {
    printf("[!] send_recv\n");
    return;
    }
  }
//}}}
//{{{
void cStLink::run_at (stm32_addr_t addr) {

  write_reg(addr, 15); /* pc register */
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
int cStLink::sendRecv (int terminate, unsigned char* txbuf, int txsize, unsigned char* rxbuf, int rxsize) {

  /* note: txbuf and rxbuf can point to the same area */
  int res = 0;

  libusb_fill_bulk_transfer (req_trans, usb_handle, ep_req, txbuf, txsize, NULL, NULL, 0);
  if (submit_wait (req_trans))
    return -1;

  /* send_only */
  if (rxsize != 0) {
    /* read the response */
    libusb_fill_bulk_transfer (rep_trans, usb_handle, ep_rep, rxbuf, rxsize, NULL, NULL, 0);
    if (submit_wait (rep_trans))
      return -1;
    res = rep_trans->actual_length;
    }

  return rep_trans->actual_length;
  }
//}}}
//{{{
int cStLink::sendOnly (int terminate, unsigned char* txbuf, int txsize) {
  return sendRecv (terminate, txbuf, txsize, NULL, 0);
  }
//}}}
//{{{
void cStLink::print_data() {

  printf ("data_len = %d 0x%x\n", q_len, q_len);

  for (int i = 0; i < q_len; i++) {
    if (i % 16 == 0) {
      //if (q_data_dir == Q_DATA_OUT)
    //    fprintf(stdout, "\n<- 0x%08x ", q_addr + i);
  //    else
//        fprintf(stdout, "\n-> 0x%08x ", q_addr + i);
      }
    printf (" %02x", (unsigned int) q_buf[i]);
    }
  printf ("\n");
  }
//}}}
//{{{
int cStLink::load_device_params() {

  printf ("Loading device parameters....\n");
  const chip_params_t *params = NULL;
  core_id = get_core_id();
  uint32_t chip_id = get_chip_id();

  chip_id = chip_id & 0xfff;
  /* Fix chip_id for F4 rev A errata , Read CPU ID, as CoreID is the same for F2/F4*/
  if (chip_id == 0x411) {
    uint32_t cpuid = read_debug32(0xE000ED00);
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
    uint32_t flash_size = read_debug32(params->flash_size_reg) & 0xffff;
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
  return q_buf[0] == STLINK_CORE_HALTED;
  }
//}}}
