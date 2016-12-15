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
#define C_BUF_LEN 32           // Enough space to hold both a V2 command or a V1 command packaged as generic scsi*/
#define Q_BUF_LEN (1024 * 100) // Max data transfer size 6kB = max mem32_read block

//{{{
typedef struct chip_params_ {
  uint32_t chip_id;
  char* description;
  uint32_t flash_size_reg;
  uint32_t flash_pagesize;
  uint32_t sram_size;
  uint32_t bootrom_base, bootrom_size;
  } chip_params_t;
//}}}
const chip_params_t devices[] = { 0x413, "F4 device", 0x1FFF7A10, 0x4000, 0x30000, 0x1fff0000, 0x7800 };

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
  int open_usb();

  void reset();
  void jtag_reset (int value);

  int current_mode  (int report);
  void enter_swd_mode();
  void enter_jtag_mode();
  void exit_debug_mode();
  void exit_dfu_mode();
  uint32_t get_core_id();

  void getStatus();
  void getVersion();

  uint32_t read_debug32 (uint32_t addr);
  void read_mem32 (uint32_t addr, uint16_t len);

  void write_debug32 (uint32_t addr, uint32_t data);
  void write_mem32 (uint32_t addr, uint16_t len);
  void write_mem8 (uint32_t addr, uint16_t len);

  void read_reg (int r_idx, reg *regp);
  void read_all_regs (reg *regp);
  void read_unsupported_reg (int r_idx, reg *regp);
  void read_all_unsupported_regs (reg *regp);

  void write_unsupported_reg (uint32_t value, int r_idx, reg *regp);
  void write_reg (uint32_t reg, int idx);

  void run();
  void run_at (stm32_addr_t addr);
  void step();
  void force_debug();

  uint32_t get_chip_id();
  void get_cpu_id  (cortex_m3_cpuid_t *cpuid);

  void get_core_stat();

  // vars
  uint32_t core_id;
  uint32_t chip_id;
  int core_stat;

  #define STM32_FLASH_PGSZ 1024
  #define STM32L_FLASH_PGSZ 256

  #define STM32F4_FLASH_PGSZ 16384
  #define STM32F4_FLASH_SIZE (128 * 1024 * 8)

  stm32_addr_t flash_base;
  size_t flash_size;
  size_t flash_pgsz;

  /* sram settings */
  #define STM32_SRAM_SIZE (8 * 1024)
  #define STM32L_SRAM_SIZE (16 * 1024)
  stm32_addr_t sram_base;
  size_t sram_size;

  // bootloader
  stm32_addr_t sys_base;
  size_t sys_size;

  struct stlink_version_ version;

private:
  unsigned int is_core_halted();

  int cStLink::submit_wait (struct libusb_transfer* trans);
  int cStLink::sendRecv (int txsize, int rxsize);
  int cStLink::sendOnly (int txsize);
  int cStLink::sendOnlyData (int txsize);
  void cStLink::print_data();
  int cStLink::load_device_params();

  libusb_device_handle* usb_handle;
  libusb_context* libusb_ctx;
  struct libusb_transfer* req_trans;
  struct libusb_transfer* rep_trans;
  unsigned int ep_req;
  unsigned int ep_rep;
  unsigned int cmd_len;

  // Room for the command header
  unsigned char c_buf[C_BUF_LEN];
  // Data transferred from or to device
  unsigned char q_buf[Q_BUF_LEN];
  int q_len;
  };
