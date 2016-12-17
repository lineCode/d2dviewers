#pragma once
#include <stdint.h>
#include "libusb.h"

//{{{  cortex m3 defines
#define CM3_REG_CPUID 0xE000ED00
#define CM3_REG_FP_CTRL 0xE0002000
#define CM3_REG_FP_COMP0 0xE0002008
//}}}
//{{{  stlink mode defines
#define STLINK_DEV_DFU_MODE   0x00
#define STLINK_DEV_MASS_MODE    0x01
#define STLINK_DEV_DEBUG_MODE   0x02
#define STLINK_DEV_UNKNOWN_MODE -1
//}}}
#define STM32_FLASH_PGSZ 1024
#define STM32L_FLASH_PGSZ 256
#define STM32F4_FLASH_PGSZ 16384
#define STM32F4_FLASH_SIZE (128 * 1024 * 8)
#define STM32_SRAM_SIZE (8 * 1024)
#define STM32L_SRAM_SIZE (16 * 1024)

#define CMD_BUF_LEN 16           // Enough space to hold both a V2 command
#define DATA_BUF_LEN (1024 * 100) // Max data transfer size 6kB = max mem32_read block

//{{{  struct reg
typedef struct {
    uint32_t r[16];
    uint32_t s[32];
    uint32_t xpsr;
    uint32_t main_sp;
    uint32_t process_sp;
    uint32_t rw;
    uint32_t rw2;
    uint8_t control;
    uint8_t faultmask;
    uint8_t basepri;
    uint8_t primask;
    uint32_t fpscr;
} reg;
//}}}
//{{{
typedef struct _cortex_m3_cpuid_ {
  uint16_t implementer_id;
  uint16_t variant;
  uint16_t part;
  uint8_t revision;
  } cortex_m3_cpuid_t;
//}}}
//{{{
typedef struct stlink_version_ {
  uint32_t stlink_v;
  uint32_t jtag_v;
  uint32_t swim_v;
  uint32_t st_vid;
  uint32_t stlink_pid;
  } stlink_version_t;
//}}}
typedef uint32_t stm32_addr_t;

class cStLink {
public:
  cStLink();
  ~cStLink();
  int openUsb();

  void reset();
  void jtagReset (int value);

  void getStatus();
  void getVersion();
  uint32_t getCoreId();
  uint32_t getChipId();
  void getCpuId  (cortex_m3_cpuid_t *cpuid);
  void getCoreStat();
  int getCurrentMode (int report);

  void enterSwdMode();
  void enterJtagMode();
  void exitDebugMode();
  void exitDfuMode();

  uint32_t readDebug32 (uint32_t addr);
  uint32_t readMem32 (uint32_t addr, uint16_t len);
  uint32_t readReg (int r_idx, reg* regp);
  void readAllRegs (reg *regp);
  void readUnsupportedReg (int r_idx, reg* regp);
  void readAllUnsupportedRegs (reg* regp);

  void writeDebug32 (uint32_t addr, uint32_t data);
  void writeMem32 (uint32_t addr, uint16_t len);
  void writeMem8 (uint32_t addr, uint16_t len);
  void writeUnsupportedReg (uint32_t value, int r_idx, reg* regp);
  void writeReg (uint32_t reg, int idx);

  void forceDebug();
  void run();
  void runAt (stm32_addr_t addr);
  void step();

  // public vars
  struct stlink_version_ version;

  uint32_t mCoreId;
  uint32_t mChipId;
  int mCoreStatus;

  stm32_addr_t flash_base;
  size_t flash_size;
  size_t flash_pgsz;

  stm32_addr_t sram_base;
  size_t sram_size;

  // bootloader
  stm32_addr_t sys_base;
  size_t sys_size;

private:
  unsigned int is_core_halted();
  int loadDeviceParams();

  int sendRecv (int txSize, int rxSize);
  int send (unsigned char* txBuf, int txSize);

  libusb_device_handle* mUsbHandle;
  libusb_context* mUsbContext;
  bool mV2;
  struct libusb_transfer* req_trans;
  struct libusb_transfer* rep_trans;
  unsigned int ep_req;
  unsigned int ep_rep;
  unsigned int ep_trace;

  unsigned int mCmdLen;
  unsigned char mCmdBuf[CMD_BUF_LEN];
  int mDataLen;
  unsigned char mDataBuf[DATA_BUF_LEN];
  };
