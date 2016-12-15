/*{{{*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "stlink-common.h"
#include "libusb.h"

/*}}}*/
/*{{{*/
/*{{{  stm32f FPEC flash controller interface, pm0063 manual*/
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
/*}}}*/
/*{{{  For STM32F05x, the RDPTR_KEY may be wrong, but as it is not used anywhere...*/
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
/*}}}*/
/*{{{  32L = 32F1 same CoreID as 32F4!*/
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
/*}}}*/
/*{{{  STM32F4*/
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
/*}}}*/
/*}}}*/
#define STLINK_CMD_SIZE 16

/*{{{*/
inline unsigned int is_bigendian(void) {
	static volatile const unsigned int i = 1;
	return *(volatile const char*) &i == 0;
	}
/*}}}*/
/*{{{*/
void write_uint32(unsigned char* buf, uint32_t ui) {
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
/*}}}*/
/*{{{*/
void write_uint16(unsigned char* buf, uint16_t ui) {
		if (!is_bigendian()) { // le -> le (don't swap)
				buf[0] = ((unsigned char*) &ui)[0];
				buf[1] = ((unsigned char*) &ui)[1];
		} else {
				buf[0] = ((unsigned char*) &ui)[1];
				buf[1] = ((unsigned char*) &ui)[0];
		}
}
/*}}}*/
/*{{{*/
uint32_t read_uint32(const unsigned char *c, const int pt) {
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
/*}}}*/

/*{{{*/

struct trans_ctx {
	#define TRANS_FLAGS_IS_DONE (1 << 0)
	#define TRANS_FLAGS_HAS_ERROR (1 << 1)
		volatile unsigned long flags;
	};
/*}}}*/
/*{{{*/
static void on_trans_done (struct libusb_transfer* trans) {

	struct trans_ctx * const ctx = trans->user_data;
	if (trans->status != LIBUSB_TRANSFER_COMPLETED)
		ctx->flags |= TRANS_FLAGS_HAS_ERROR;
	ctx->flags |= TRANS_FLAGS_IS_DONE;
	}
/*}}}*/
/*{{{*/
int submit_wait (stlink_t* sl, struct libusb_transfer* trans) {

	struct trans_ctx trans_ctx;
	enum libusb_error error;

	trans_ctx.flags = 0;

	/* brief intrusion inside the libusb interface */
	trans->callback = on_trans_done;
	trans->user_data = &trans_ctx;

	if ((error = libusb_submit_transfer (trans))) {
		printf ("libusb_submit_transfer(%d)\n", error);
		return -1;
		}

	while (trans_ctx.flags == 0) {
		struct timeval timeout;
		timeout.tv_sec = 3;
		timeout.tv_usec = 0;
		if (libusb_handle_events_timeout (sl->libusb_ctx, &timeout)) {
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
/*}}}*/

/*{{{*/
ssize_t send_recv (stlink_t* sl, int terminate,
									 unsigned char* txbuf, size_t txsize, unsigned char* rxbuf, size_t rxsize) {

	/* note: txbuf and rxbuf can point to the same area */
	int res = 0;

	libusb_fill_bulk_transfer (sl->req_trans, sl->usb_handle, sl->ep_req, txbuf, txsize, NULL, NULL, 0);
	if (submit_wait (sl, sl->req_trans))
		return -1;

	/* send_only */
	if (rxsize != 0) {
		/* read the response */
		libusb_fill_bulk_transfer (sl->rep_trans, sl->usb_handle, sl->ep_rep, rxbuf, rxsize, NULL, NULL, 0);
		if (submit_wait (sl, sl->rep_trans))
			return -1;
		res = sl->rep_trans->actual_length;
		}

	return sl->rep_trans->actual_length;
	}
/*}}}*/
/*{{{*/
static int send_only (stlink_t* sl, int terminate, unsigned char* txbuf, size_t txsize) {
	return send_recv (sl, terminate, txbuf, txsize, NULL, 0);
	}
/*}}}*/

/*{{{*/
void stlink_usb_close (stlink_t* sl) {

	if (sl != NULL) {
		if (sl->req_trans != NULL)
			libusb_free_transfer (sl->req_trans);

		 if (sl->rep_trans != NULL)
			 libusb_free_transfer (sl->rep_trans);

		 if (sl->usb_handle != NULL)
			 libusb_close (sl->usb_handle);

		 libusb_exit (sl->libusb_ctx);
		 }
	}
/*}}}*/
/*{{{*/
void stlink_usb_version (stlink_t *sl) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;
		ssize_t size;
		uint32_t rep_len = 6;
		int i = 0;
		cmd[i++] = STLINK_GET_VERSION;

		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
uint32_t stlink_usb_read_debug32 (stlink_t *sl, uint32_t addr) {

		unsigned char* const rdata = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;
		ssize_t size;
		const int rep_len = 8;

		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_JTAG_READDEBUG_32BIT;
		write_uint32(&cmd[i], addr);
		size = send_recv(sl, 1, cmd, sl->cmd_len, rdata, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return 0;
		}
		return read_uint32(rdata, 4);
}
/*}}}*/
/*{{{*/
void stlink_usb_write_debug32 (stlink_t *sl, uint32_t addr, uint32_t data) {
		unsigned char* const rdata = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;
		ssize_t size;
		const int rep_len = 2;

		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_JTAG_WRITEDEBUG_32BIT;
		write_uint32(&cmd[i], addr);
		write_uint32(&cmd[i + 4], data);
		size = send_recv(sl, 1, cmd, sl->cmd_len, rdata, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
void stlink_usb_write_mem32 (stlink_t *sl, uint32_t addr, uint16_t len) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;

		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_WRITEMEM_32BIT;
		write_uint32(&cmd[i], addr);
		write_uint16(&cmd[i + 4], len);
		send_only(sl, 0, cmd, sl->cmd_len);

		send_only(sl, 1, data, len);
}
/*}}}*/
/*{{{*/
void stlink_usb_write_mem8 (stlink_t *sl, uint32_t addr, uint16_t len) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;

		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_WRITEMEM_8BIT;
		write_uint32(&cmd[i], addr);
		write_uint16(&cmd[i + 4], len);
		send_only(sl, 0, cmd, sl->cmd_len);
		send_only(sl, 1, data, len);
}
/*}}}*/
/*{{{*/
int stlink_usb_current_mode (stlink_t * sl) {
		unsigned char* const cmd  = sl->c_buf;
		unsigned char* const data = sl->q_buf;
		 ssize_t size;
		int rep_len = 2;
		int i = 0;
		cmd[i++] = STLINK_GET_CURRENT_MODE;
		size = send_recv(sl, 1, cmd,  sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return -1;
		}
		return sl->q_buf[0];
}

/*}}}*/
/*{{{*/
void stlink_usb_core_id (stlink_t * sl) {
		unsigned char* const cmd  = sl->c_buf;
		unsigned char* const data = sl->q_buf;
		ssize_t size;
		int rep_len = 4;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_READCOREID;

		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}

		sl->core_id = read_uint32(data, 0);
}
/*}}}*/
/*{{{*/
void stlink_usb_status (stlink_t * sl) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;
		ssize_t size;
		int rep_len = 2;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_GETSTATUS;

		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
void stlink_usb_force_debug (stlink_t *sl) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;
		ssize_t size;
		int rep_len = 2;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_FORCEDEBUG;
		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
void stlink_usb_enter_swd_mode (stlink_t * sl) {
		unsigned char* const cmd  = sl->c_buf;
		ssize_t size;
		const int rep_len = 0;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_ENTER;
		cmd[i++] = STLINK_DEBUG_ENTER_SWD;

		size = send_only(sl, 1, cmd, sl->cmd_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
void stlink_usb_exit_dfu_mode (stlink_t* sl) {
		unsigned char* const cmd = sl->c_buf;
		ssize_t size;
		int i = 0;
		cmd[i++] = STLINK_DFU_COMMAND;
		cmd[i++] = STLINK_DFU_EXIT;

		size = send_only(sl, 1, cmd, sl->cmd_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
/**
 * TODO - not convinced this does anything...
 * @param sl
 */
void stlink_usb_reset(stlink_t * sl) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd = sl->c_buf;
		ssize_t size;
		int rep_len = 2;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_RESETSYS;

		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
void stlink_usb_jtag_reset (stlink_t * sl, int value) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd = sl->c_buf;
		ssize_t size;
		int rep_len = 2;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_JTAG_DRIVE_NRST;
		cmd[i++] = (value)?0:1;

		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
void stlink_usb_step (stlink_t* sl) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd = sl->c_buf;
		ssize_t size;
		int rep_len = 2;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_STEPCORE;

		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}

/*}}}*/
/*{{{*/
/**
 * This seems to do a good job of restarting things from the beginning?
 * @param sl
 */
void stlink_usb_run (stlink_t* sl) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd = sl->c_buf;
		ssize_t size;
		int rep_len = 2;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_RUNCORE;

		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
}
/*}}}*/
/*{{{*/
void stlink_usb_exit_debug_mode (stlink_t *sl) {
		unsigned char* const cmd = sl->c_buf;
		ssize_t size;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_EXIT;

		size = send_only(sl, 1, cmd, sl->cmd_len);
		if (size == -1) {
				printf("[!] send_only\n");
				return;
		}
}
/*}}}*/
/*{{{*/
void stlink_usb_read_mem32 (stlink_t *sl, uint32_t addr, uint16_t len) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd = sl->c_buf;
		ssize_t size;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_READMEM_32BIT;
		write_uint32(&cmd[i], addr);
		write_uint16(&cmd[i + 4], len);

		size = send_recv(sl, 1, cmd, sl->cmd_len, data, len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}

		sl->q_len = (size_t) size;

		stlink_print_data(sl);
}
/*}}}*/
/*{{{*/
void stlink_usb_read_all_regs (stlink_t *sl, reg *regp) {
		unsigned char* const cmd = sl->c_buf;
		unsigned char* const data = sl->q_buf;
		ssize_t size;
		uint32_t rep_len = 84;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_READALLREGS;
		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
		sl->q_len = (size_t) size;
		stlink_print_data(sl);
		for(i=0; i<16; i++)
				regp->r[i]= read_uint32(sl->q_buf, i*4);
		regp->xpsr       = read_uint32(sl->q_buf, 64);
		regp->main_sp    = read_uint32(sl->q_buf, 68);
		regp->process_sp = read_uint32(sl->q_buf, 72);
		regp->rw         = read_uint32(sl->q_buf, 76);
		regp->rw2        = read_uint32(sl->q_buf, 80);
		if (sl->verbose < 2)
				return;

		printf ("xpsr       = 0x%08x\n", read_uint32(sl->q_buf, 64));
		printf ("main_sp    = 0x%08x\n", read_uint32(sl->q_buf, 68));
		printf ("process_sp = 0x%08x\n", read_uint32(sl->q_buf, 72));
		printf ("rw         = 0x%08x\n", read_uint32(sl->q_buf, 76));
		printf ("rw2        = 0x%08x\n", read_uint32(sl->q_buf, 80));
}
/*}}}*/
/*{{{*/
void stlink_usb_read_reg (stlink_t *sl, int r_idx, reg *regp) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;
		ssize_t size;
		uint32_t r;
		uint32_t rep_len = 4;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_READREG;
		cmd[i++] = (uint8_t) r_idx;
		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
		sl->q_len = (size_t) size;
		stlink_print_data(sl);
		r = read_uint32(sl->q_buf, 0);
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
/*}}}*/
/*{{{*/
/* See section C1.6 of the ARMv7-M Architecture Reference Manual */
void stlink_usb_read_unsupported_reg (stlink_t *sl, int r_idx, reg *regp) {
		uint32_t r;

		sl->q_buf[0] = (unsigned char) r_idx;
		for (int i = 1; i < 4; i++) {
				sl->q_buf[i] = 0;
		}

		stlink_usb_write_mem32 (sl, DCRSR, 4);
		stlink_usb_read_mem32 (sl, DCRDR, 4);

		r = read_uint32 (sl->q_buf, 0);
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
/*}}}*/
/*{{{*/
void stlink_usb_read_all_unsupported_regs (stlink_t *sl, reg *regp) {
		stlink_usb_read_unsupported_reg(sl, 0x14, regp);
		stlink_usb_read_unsupported_reg(sl, 0x21, regp);

		for (int i = 0; i < 32; i++) {
				stlink_usb_read_unsupported_reg(sl, 0x40+i, regp);
		}
}
/*}}}*/
/*{{{*/
/* See section C1.6 of the ARMv7-M Architecture Reference Manual */
void stlink_usb_write_unsupported_reg (stlink_t *sl, uint32_t val, int r_idx, reg *regp) {
		if (r_idx >= 0x1C && r_idx <= 0x1F) { /* primask, basepri, faultmask, or control */
				/* These are held in the same register */
				stlink_usb_read_unsupported_reg(sl, 0x14, regp);

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

		write_uint32(sl->q_buf, val);

		stlink_usb_write_mem32(sl, DCRDR, 4);

		sl->q_buf[0] = (unsigned char) r_idx;
		sl->q_buf[1] = 0;
		sl->q_buf[2] = 0x01;
		sl->q_buf[3] = 0;

		stlink_usb_write_mem32(sl, DCRSR, 4);
}
/*}}}*/
/*{{{*/
void stlink_usb_write_reg (stlink_t *sl, uint32_t reg, int idx) {
		unsigned char* const data = sl->q_buf;
		unsigned char* const cmd  = sl->c_buf;
		ssize_t size;
		uint32_t rep_len = 2;
		int i = 0;
		cmd[i++] = STLINK_DEBUG_COMMAND;
		cmd[i++] = STLINK_DEBUG_WRITEREG;
		cmd[i++] = idx;
		write_uint32(&cmd[i], reg);
		size = send_recv(sl, 1, cmd, sl->cmd_len, data, rep_len);
		if (size == -1) {
				printf("[!] send_recv\n");
				return;
		}
		sl->q_len = (size_t) size;
		stlink_print_data(sl);
}
/*}}}*/

/*{{{*/
void stlink_close (stlink_t* sl) {
	stlink_usb_close(sl);
	}
/*}}}*/
/*{{{*/
void stlink_exit_debug_mode(stlink_t *sl) {
	printf ("stlink_exit_debug_mode\n");
	stlink_write_debug32(sl, DHCSR, DBGKEY);
	stlink_usb_exit_debug_mode(sl);
	}
/*}}}*/
/*{{{*/
void stlink_enter_swd_mode(stlink_t *sl) {
	printf ("Stlink_enter_swd_mode\n");
	stlink_usb_enter_swd_mode(sl);
	}
/*}}}*/
/*{{{*/
// Force the core into the debug mode -> halted state.
void stlink_force_debug(stlink_t *sl) {
	printf  ("stlink_force_debug_mode\n");
	stlink_usb_force_debug(sl);
	}
/*}}}*/
/*{{{*/
void stlink_exit_dfu_mode(stlink_t *sl) {
		printf ("stlink_exit_dfu_mode\n");
		stlink_usb_exit_dfu_mode(sl);
}
/*}}}*/
/*{{{*/

uint32_t stlink_core_id(stlink_t *sl) {
		stlink_usb_core_id(sl);
		if (sl->verbose > 2)
				stlink_print_data(sl);
		printf ("core_id = 0x%08x\n", sl->core_id);
		return sl->core_id;
}
/*}}}*/
/*{{{*/
uint32_t stlink_chip_id(stlink_t *sl) {
		uint32_t chip_id = stlink_read_debug32(sl, 0xE0042000);
		if (chip_id == 0) chip_id = stlink_read_debug32(sl, 0x40015800);  //Try Corex M0 DBGMCU_IDCODE register address
		return chip_id;
}
/*}}}*/
/*{{{*/
/**
 * Cortex m3 tech ref manual, CPUID register description
 * @param sl stlink context
 * @param cpuid pointer to the result object
 */
void stlink_cpu_id(stlink_t *sl, cortex_m3_cpuid_t *cpuid) {
		uint32_t raw = stlink_read_debug32(sl, CM3_REG_CPUID);
		cpuid->implementer_id = (raw >> 24) & 0x7f;
		cpuid->variant = (raw >> 20) & 0xf;
		cpuid->part = (raw >> 4) & 0xfff;
		cpuid->revision = raw & 0xf;
		return;
}
/*}}}*/
/*{{{*/
/**
 * reads and decodes the flash parameters, as dynamically as possible
 * @param sl
 * @return 0 for success, or -1 for unsupported core type.
 */
int stlink_load_device_params(stlink_t *sl) {
		printf ("Loading device parameters....\n");
		const chip_params_t *params = NULL;
		sl->core_id = stlink_core_id(sl);
		uint32_t chip_id = stlink_chip_id(sl);

		sl->chip_id = chip_id & 0xfff;
		/* Fix chip_id for F4 rev A errata , Read CPU ID, as CoreID is the same for F2/F4*/
		if (sl->chip_id == 0x411) {
				uint32_t cpuid = stlink_read_debug32(sl, 0xE000ED00);
				if ((cpuid  & 0xfff0) == 0xc240)
						sl->chip_id = 0x413;
		}

		for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
				if(devices[i].chip_id == sl->chip_id) {
						params = &devices[i];
						break;
				}
		}
		if (params == NULL) {
				printf ("unknown chip id! %#x\n", chip_id);
				return -1;
		}

		// These are fixed...
		sl->flash_base = STM32_FLASH_BASE;
		sl->sram_base = STM32_SRAM_BASE;

		// read flash size from hardware, if possible...
		if (sl->chip_id == STM32_CHIPID_F2) {
				sl->flash_size = 0x100000; /* Use maximum, User must care!*/
		} else if (sl->chip_id == STM32_CHIPID_F4) {
		sl->flash_size = 0x100000;      //todo: RM0090 error; size register same address as unique ID
		} else {
				uint32_t flash_size = stlink_read_debug32(sl, params->flash_size_reg) & 0xffff;
				sl->flash_size = flash_size * 1024;
		}
		sl->flash_pgsz = params->flash_pagesize;
		sl->sram_size = params->sram_size;
		sl->sys_base = params->bootrom_base;
		sl->sys_size = params->bootrom_size;

		printf ("Device connected is: %s, id %#x\n", params->description, chip_id);
		// TODO make note of variable page size here.....
		printf ("SRAM size: %#x bytes (%d KiB), Flash: %#x bytes (%d KiB) in pages of %zd bytes\n",
				sl->sram_size, sl->sram_size / 1024, sl->flash_size, sl->flash_size / 1024,
				sl->flash_pgsz);
		return 0;
}
/*}}}*/
/*{{{*/
void stlink_reset(stlink_t *sl) {
		printf ("*** stlink_reset ***\n");
		stlink_usb_reset(sl);
}
/*}}}*/
/*{{{*/
void stlink_jtag_reset(stlink_t *sl, int value) {
		printf ("*** stlink_jtag_reset ***\n");
		stlink_usb_jtag_reset(sl, value);
}
/*}}}*/
/*{{{*/
void stlink_run(stlink_t *sl) {
		printf ("*** stlink_run ***\n");
		stlink_usb_run(sl);
}
/*}}}*/
/*{{{*/
void stlink_status(stlink_t *sl) {
		printf ("*** stlink_status ***\n");
		stlink_usb_status(sl);
		stlink_core_stat(sl);
}
/*}}}*/
/*{{{*/
/**
 * Decode the version bits, originally from -sg, verified with usb
 * @param sl stlink context, assumed to contain valid data in the buffer
 * @param slv output parsed version object
 */
void _parse_version(stlink_t *sl, stlink_version_t *slv) {
		uint32_t b0 = sl->q_buf[0]; //lsb
		uint32_t b1 = sl->q_buf[1];
		uint32_t b2 = sl->q_buf[2];
		uint32_t b3 = sl->q_buf[3];
		uint32_t b4 = sl->q_buf[4];
		uint32_t b5 = sl->q_buf[5]; //msb

		// b0 b1                       || b2 b3  | b4 b5
		// 4b        | 6b     | 6b     || 2B     | 2B
		// stlink_v  | jtag_v | swim_v || st_vid | stlink_pid

		slv->stlink_v = (b0 & 0xf0) >> 4;
		slv->jtag_v = ((b0 & 0x0f) << 2) | ((b1 & 0xc0) >> 6);
		slv->swim_v = b1 & 0x3f;
		slv->st_vid = (b3 << 8) | b2;
		slv->stlink_pid = (b5 << 8) | b4;
		return;
}
/*}}}*/
/*{{{*/
void stlink_version(stlink_t *sl) {
		printf ("*** looking up stlink version\n");
		stlink_usb_version(sl);
		_parse_version(sl, &sl->version);

		printf ("st vid         = 0x%04x (expect 0x%04x)\n", sl->version.st_vid, USB_ST_VID);
		printf ("stlink pid     = 0x%04x\n", sl->version.stlink_pid);
		printf ("stlink version = 0x%x\n", sl->version.stlink_v);
		printf ("jtag version   = 0x%x\n", sl->version.jtag_v);
		printf ("swim version   = 0x%x\n", sl->version.swim_v);
		if (sl->version.jtag_v == 0) {
				printf ("    notice: the firmware doesn't support a jtag/swd interface\n");
		}
		if (sl->version.swim_v == 0) {
				printf ("    notice: the firmware doesn't support a swim interface\n");
		}
}
/*}}}*/
/*{{{*/
uint32_t stlink_read_debug32(stlink_t *sl, uint32_t addr) {
		uint32_t data = stlink_usb_read_debug32(sl, addr);
		printf ("*** stlink_read_debug32 %x is %#x\n", data, addr);
		return data;
}
/*}}}*/
/*{{{*/
void stlink_write_debug32(stlink_t *sl, uint32_t addr, uint32_t data) {
		printf ("*** stlink_write_debug32 %x to %#x\n", data, addr);
		stlink_usb_write_debug32(sl, addr, data);
}
/*}}}*/
/*{{{*/
void stlink_write_mem32(stlink_t *sl, uint32_t addr, uint16_t len) {
		printf ("*** stlink_write_mem32 %u bytes to %#x\n", len, addr);
		if (len % 4 != 0) {
				fprintf(stderr, "Error: Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
				abort();
		}
		stlink_usb_write_mem32(sl, addr, len);
}
/*}}}*/
/*{{{*/
void stlink_read_mem32(stlink_t *sl, uint32_t addr, uint16_t len) {
		if (len % 4 != 0) { // !!! never ever: fw gives just wrong values
				fprintf(stderr, "Error: Data length doesn't have a 32 bit alignment: +%d byte.\n",
								len % 4);
				abort();
		}
		stlink_usb_read_mem32(sl, addr, len);
}
/*}}}*/
/*{{{*/
void stlink_write_mem8(stlink_t *sl, uint32_t addr, uint16_t len) {
		printf ("*** stlink_write_mem8 ***\n");
		if (len > 0x40 ) { // !!! never ever: Writing more then 0x40 bytes gives unexpected behaviour
				fprintf(stderr, "Error: Data length > 64: +%d byte.\n",
								len);
				abort();
		}
		stlink_usb_write_mem8(sl, addr, len);
}
/*}}}*/
/*{{{*/
void stlink_read_all_regs(stlink_t *sl, reg *regp) {
		printf ("*** stlink_read_all_regs ***\n");
		stlink_usb_read_all_regs(sl, regp);
}
/*}}}*/
/*{{{*/
void stlink_read_all_unsupported_regs(stlink_t *sl, reg *regp) {
		printf ("*** stlink_read_all_unsupported_regs ***\n");
		stlink_usb_read_all_unsupported_regs(sl, regp);
}
/*}}}*/
/*{{{*/
void stlink_write_reg(stlink_t *sl, uint32_t reg, int idx) {
		printf ("*** stlink_write_reg\n");
		stlink_usb_write_reg(sl, reg, idx);
}
/*}}}*/
/*{{{*/
void stlink_read_reg(stlink_t *sl, int r_idx, reg *regp) {
		printf (" (%d) ***\n", r_idx);
		if (r_idx > 20 || r_idx < 0) {
				fprintf(stderr, "Error: register index must be in [0..20]\n");
				return;
		}

		stlink_usb_read_reg(sl, r_idx, regp);
}
/*}}}*/
/*{{{*/
void stlink_read_unsupported_reg(stlink_t *sl, int r_idx, reg *regp) {
		int r_convert;

		printf ("*** stlink_read_unsupported_reg\n");
		printf (" (%d) ***\n", r_idx);

		/* Convert to values used by DCRSR */
		if (r_idx >= 0x1C && r_idx <= 0x1F) { /* primask, basepri, faultmask, or control */
				r_convert = 0x14;
		} else if (r_idx == 0x40) {     /* FPSCR */
				r_convert = 0x21;
		} else if (r_idx >= 0x20 && r_idx < 0x40) {
				r_convert = 0x40 + (r_idx - 0x20);
		} else {
				fprintf(stderr, "Error: register address must be in [0x1C..0x40]\n");
				return;
		}

		stlink_usb_read_unsupported_reg(sl, r_convert, regp);
}
/*}}}*/
/*{{{*/
void stlink_write_unsupported_reg(stlink_t *sl, uint32_t val, int r_idx, reg *regp) {
		int r_convert;

		printf ("*** stlink_write_unsupported_reg\n");
		printf (" (%d) ***\n", r_idx);

		/* Convert to values used by DCRSR */
		if (r_idx >= 0x1C && r_idx <= 0x1F) { /* primask, basepri, faultmask, or control */
				r_convert = r_idx;  /* The backend function handles this */
		} else if (r_idx == 0x40) {     /* FPSCR */
				r_convert = 0x21;
		} else if (r_idx >= 0x20 && r_idx < 0x40) {
				r_convert = 0x40 + (r_idx - 0x20);
		} else {
				fprintf(stderr, "Error: register address must be in [0x1C..0x40]\n");
				return;
		}

		stlink_usb_write_unsupported_reg(sl, val, r_convert, regp);
}
/*}}}*/
/*{{{*/
unsigned int is_core_halted(stlink_t *sl) {
		/* return non zero if core is halted */
		stlink_status(sl);
		return sl->q_buf[0] == STLINK_CORE_HALTED;
}
/*}}}*/
/*{{{*/
void stlink_step(stlink_t *sl) {
		printf ("*** stlink_step ***\n");
		stlink_usb_step(sl);
}
/*}}}*/
/*{{{*/
int stlink_current_mode(stlink_t* sl) {
		int mode = stlink_usb_current_mode(sl);
		switch (mode) {
				case STLINK_DEV_DFU_MODE:
						printf ("stlink current mode: dfu\n");
						return mode;
				case STLINK_DEV_DEBUG_MODE:
						printf ("stlink current mode: debug (jtag or swd)\n");
						return mode;
				case STLINK_DEV_MASS_MODE:
						printf ("stlink current mode: mass\n");
						return mode;
		}
		printf ("stlink mode: unknown!\n");
		return STLINK_DEV_UNKNOWN_MODE;
}
/*}}}*/

/*{{{*/
// same as above with entrypoint.

void stlink_run_at(stlink_t *sl, stm32_addr_t addr) {
		stlink_write_reg(sl, addr, 15); /* pc register */

		stlink_run(sl);

		while (is_core_halted(sl) == 0)
				Sleep(3000000);
}
/*}}}*/
/*{{{*/
void stlink_core_stat(stlink_t *sl) {
		if (sl->q_len <= 0)
				return;

		switch (sl->q_buf[0]) {
				case STLINK_CORE_RUNNING:
						sl->core_stat = STLINK_CORE_RUNNING;
						printf ("  core status: running\n");
						return;
				case STLINK_CORE_HALTED:
						sl->core_stat = STLINK_CORE_HALTED;
						printf ("  core status: halted\n");
						return;
				default:
						sl->core_stat = STLINK_CORE_STAT_UNKNOWN;
						fprintf(stderr, "  core status: unknown\n");
		}
}
/*}}}*/
/*{{{*/
void stlink_print_data(stlink_t * sl) {
		if (sl->verbose > 2)
				fprintf(stdout, "data_len = %d 0x%x\n", sl->q_len, sl->q_len);

		for (int i = 0; i < sl->q_len; i++) {
				if (i % 16 == 0) {
						/*
																		if (sl->q_data_dir == Q_DATA_OUT)
																						fprintf(stdout, "\n<- 0x%08x ", sl->q_addr + i);
																		else
																						fprintf(stdout, "\n-> 0x%08x ", sl->q_addr + i);
						 */
				}
				fprintf(stdout, " %02x", (unsigned int) sl->q_buf[i]);
		}
		fputs("\n\n", stdout);
}
/*}}}*/

/*{{{*/
stlink_t* stlink_open_usb() {

	stlink_t* sl = malloc(sizeof (stlink_t));
	memset (sl, 0, sizeof (stlink_t));

	sl->core_stat = STLINK_CORE_STAT_UNKNOWN;

	if (libusb_init (&(sl->libusb_ctx))) {
		printf ("failed to init libusb context, wrong version of libraries?\n");
		return 0;
		}

	sl->usb_handle = libusb_open_device_with_vid_pid (sl->libusb_ctx, USB_ST_VID, USB_STLINK_V21_PID);
	if (sl->usb_handle == NULL) {
		sl->usb_handle = libusb_open_device_with_vid_pid (sl->libusb_ctx, USB_ST_VID, USB_STLINK_32L_PID);
		if (sl->usb_handle == NULL) {
			printf ("Couldn't find any ST-Link/V2 devices\n");
			return 0;
			}
		else
			printf ("st link v2 found\n");
		}
	else
		printf ("st link v21 found\n");

	if (libusb_kernel_driver_active (sl->usb_handle, 0) == 1) {
		int r;
		r = libusb_detach_kernel_driver (sl->usb_handle, 0);
		 if (r < 0) {
			 printf  ("libusb_detach_kernel_driver(() error %s\n", strerror(-r));
			 return 0;
			 }
		 }

	int config;
	if (libusb_get_configuration (sl->usb_handle, &config)) {
		/* this may fail for a previous configured device */
		printf ("libusb_get_configuration()\n");
		return 0;
		}

	if (config != 1) {
		printf ("setting new configuration (%d -> 1)\n", config);
		if (libusb_set_configuration(sl->usb_handle, 1)) {
			/* this may fail for a previous configured device */
			printf ("libusb_set_configuration() failed\n");
			return 0;
			}
		}

	if (libusb_claim_interface (sl->usb_handle, 0)) {
		printf ("libusb_claim_interface() failed\n");
		return 0;
		}

	sl->req_trans = libusb_alloc_transfer(0);
	if (sl->req_trans == NULL) {
		printf ("libusb_alloc_transfer failed\n");
		return 0;
		}

	sl->rep_trans = libusb_alloc_transfer(0);
	if (sl->rep_trans == NULL) {
		printf ("libusb_alloc_transfer failed\n");
		return 0;
		}

	// TODO - could use the scanning techniq from stm8 code here...
	sl->ep_rep = 1 /* ep rep */ | LIBUSB_ENDPOINT_IN;
	sl->ep_req = 2 /* ep req */ | LIBUSB_ENDPOINT_OUT;
	// TODO - never used at the moment, always CMD_SIZE
	sl->cmd_len = STLINK_CMD_SIZE;

	/* success */
	if (stlink_current_mode(sl) == STLINK_DEV_DFU_MODE) {
		printf ("-- exit_dfu_mode\n");
		stlink_exit_dfu_mode(sl);
		}

	if (stlink_current_mode(sl) != STLINK_DEV_DEBUG_MODE) {
		stlink_enter_swd_mode(sl);
		}

	stlink_reset(sl);
	stlink_load_device_params(sl);
	stlink_version(sl);

	return sl;
	}
/*}}}*/
