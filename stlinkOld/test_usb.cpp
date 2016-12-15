#include <stdio.h>
#include "cStLink.h"

int main (int ac, char** av) {

  cStLink stlink;
  stlink.open_usb();

  printf ("mode before doing anything: %d\n", stlink.current_mode(0));
  if (stlink.current_mode (0) == STLINK_DEV_DFU_MODE) {
    printf ("-- exit_dfu_mode\n");
    stlink.exit_dfu_mode ();
    }
  printf ("-- enter_swd_mode\n");
  stlink.enter_swd_mode ();

  printf ("-- mode after entering swd mode: %d\n", stlink.current_mode(0));
  printf ("chip id: %#x, core_id: %#x\n", stlink.chip_id, stlink.core_id);

  cortex_m3_cpuid_t cpuid;
  stlink.get_cpu_id (&cpuid);
  printf ("Cpuid implId:%0#x, variant:%#x part:%#x, rev:%#x\n",
          cpuid.implementer_id, cpuid.variant, cpuid.part, cpuid.revision);

  printf ("Read sram\n");
  static const uint32_t sram_base = 0x8000000;
  uint32_t off;
  for (off = 0; off < 16; off += 4)
    stlink.read_mem32 (sram_base + off, 4);

  printf ("FP_CTRL\n");
  stlink.read_mem32 (CM3_REG_FP_CTRL, 4);

  reg regs;
  stlink.read_all_regs (&regs);

  printf ("-- status\n");
  stlink.getStatus ();

  printf ("-- run\n");
  stlink.run ();

  printf ("-- exit_debug_mode\n");
  stlink.exit_debug_mode ();

  Sleep (100000);
  return 0;
  }
