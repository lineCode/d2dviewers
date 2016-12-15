/*{{{*/
#include <stdio.h>
#include "stlink-common.h"
/*}}}*/

int main (int ac, char** av) {

  stlink_t* sl = stlink_open_usb();
  if (sl != NULL) {
    printf ("mode before doing anything: %d\n", stlink_current_mode(sl));
    if (stlink_current_mode (sl) == STLINK_DEV_DFU_MODE) {
      printf ("-- exit_dfu_mode\n");
      stlink_exit_dfu_mode (sl);
      }

    printf ("-- enter_swd_mode\n");
    stlink_enter_swd_mode (sl);

    printf ("-- mode after entering swd mode: %d\n", stlink_current_mode(sl));
    printf ("-- chip id: %#x\n", sl->chip_id);
    printf ("-- core_id: %#x\n", sl->core_id);

    cortex_m3_cpuid_t cpuid;
    stlink_cpu_id (sl, &cpuid);
    printf ("cpuid:impl_id = %0#x, variant = %#x\n", cpuid.implementer_id, cpuid.variant);
    printf ("cpuid:part = %#x, rev = %#x\n", cpuid.part, cpuid.revision);

    printf ("-- read_sram\n");
    static const uint32_t sram_base = 0x8000000;
    uint32_t off;
    for (off = 0; off < 16; off += 4)
      stlink_read_mem32 (sl, sram_base + off, 4);

    printf ("FP_CTRL\n");
    stlink_read_mem32 (sl, CM3_REG_FP_CTRL, 4);

    reg regs;
    stlink_read_all_regs (sl, &regs);

    printf ("-- status\n");
    stlink_status (sl);

    printf ("-- run\n");
    stlink_run (sl);

    printf ("-- exit_debug_mode\n");
    stlink_exit_debug_mode (sl);

    stlink_close(sl);
    free (sl);
    }

  Sleep (100000);
  return 0;
  }
