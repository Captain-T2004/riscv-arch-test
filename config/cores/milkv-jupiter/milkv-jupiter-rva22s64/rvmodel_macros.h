// rvmodel_macros.h
// DUT-specific macro definitions for SpacemiT K1 / Milk-V Jupiter
// SPDX-License-Identifier: BSD-3-Clause
//
// Halt mechanism: UART-based pass/fail signaling, then WFI spin.
// The firmware runner (Task 2) monitors the UART output for the
// magic bytes written by RVMODEL_HALT_PASS / RVMODEL_HALT_FAIL.
//
// Hardware addresses (SpacemiT K1):
//   UART0 (NS16550-compatible): 0xD4017000
//   Machine timer (mtime):      0xC4000008
//   Machine timer cmp:          0xC4000020
//   MSIP:                       0xC4000000
//
// Memory layout (see link.ld):
//   DRAM base: 0x00000000; test ELF loaded at 0x00400000.
//   Adjust RVMODEL_ELF_LOAD_ADDR if firmware places tests elsewhere.

#ifndef _RVMODEL_MACROS_H
#define _RVMODEL_MACROS_H

// K1 UART0 (NS16550 compatible)
.EQU K1_UART0_BASE,  0xD4017000
.EQU K1_UART_THR,   (K1_UART0_BASE + 0x00)  // Transmit Holding Register
.EQU K1_UART_LSR,   (K1_UART0_BASE + 0x14)  // Line Status Register
.EQU K1_UART_LSR_THRE, 0x20                  // Bit 5: TX Holding Register Empty

// K1 machine timer (CLINT-compatible interface)
.EQU K1_MTIME_ADDRESS,    0xC4000008
.EQU K1_MTIMECMP_ADDRESS, 0xC4000020
.EQU K1_MSIP_ADDRESS,     0xC4000000

#define RVMODEL_DATA_SECTION \
    .pushsection .data,"aw",@progbits;  \
    .popsection

#define STANDARD_SM_SUPPORTED

##### STARTUP #####

//#define RVMODEL_BOOT

//#define RVMODEL_BOOT_TO_MMODE

##### TERMINATION #####

// Halt with a PASS result.
// Calls rvmodel_io_write_str (always defined by rvtest_setup.h) with a0
// pointing to "PASS\n\0", then spins in WFI.
// Using `call` (auipc+jalr) avoids undefined-reference errors that arose
// from the previous `j _rvmodel_uart_puts` approach where
// RVMODEL_UART_PUTS_ROUTINE was never invoked to define the label.
#define RVMODEL_HALT_PASS                              \
    la   a0, 1f                                       ;\
    call rvmodel_io_write_str                         ;\
    _rvmodel_halt_pass_loop:                          ;\
    wfi                                               ;\
    j    _rvmodel_halt_pass_loop                      ;\
1:  .ascii "PASS\n\0"                                 ;\
    .balign 4

// Halt with a FAIL result.
// Same approach as RVMODEL_HALT_PASS but writes "FAIL\n".
#define RVMODEL_HALT_FAIL                              \
    la   a0, 1f                                       ;\
    call rvmodel_io_write_str                         ;\
    _rvmodel_halt_fail_loop:                          ;\
    wfi                                               ;\
    j    _rvmodel_halt_fail_loop                      ;\
1:  .ascii "FAIL\n\0"                                 ;\
    .balign 4

##### IO #####

// UART0 on K1 may already be initialized by OpenSBI/U-Boot.
// If running before any firmware, initialize here.

/*
  Note the known gap: li _R1, K1_UART0_BASE loads 0xD4017000. On RV64, li
  sign-extends. Since bit 31 of 0xD4017000 is set, this expands to 0xFFFFFFFFD4017000,
  which is a completely wrong address. This is fine for ELF compilation (no error)
  but will silently send UART output to the wrong place on hardware. Fixing it
  requires a lui + addi sequence that zero-extends.
*/
#define RVMODEL_IO_INIT(_R1, _R2, _R3)                \
    li   _R1, K1_UART0_BASE                          ;\
    li   _R2, 0x03                                   ;\
    sb   _R2, 0x0C(_R1)                              ; /* LCR: 8N1 */

#define RVMODEL_IO_WRITE_STR(_R1, _R2, _R3, _STR_PTR) \
1:                                                    ;\
    lbu  _R1, 0(_STR_PTR)                            ;\
    beqz _R1, 3f                                     ;\
2:                                                    ;\
    li   _R2, K1_UART_LSR                            ;\
4:                                                    ;\
    lbu  _R3, 0(_R2)                                 ;\
    andi _R3, _R3, K1_UART_LSR_THRE                 ;\
    beqz _R3, 4b                                     ;\
    li   _R2, K1_UART_THR                            ;\
    sb   _R1, 0(_R2)                                 ;\
    addi _STR_PTR, _STR_PTR, 1                       ;\
    j    1b                                          ;\
3:

##### Access Fault #####

// 0x40000000 is unmapped in both the Sail memory model and real K1 hardware:
//   - Sail model maps DRAM as 0x00000000-0x3FFFFFFF (1GB); 0x40000000 is in the gap.
//   - K1 SoC: gap between end of DRAM mapping and peripheral space (0xC0000000+).
// Medany auipc check: 0x40000000 has bit 31 clear (zero-extends to +0x40000000),
//   pcrel from 0x80000000 = -1GB, well within the ±2GB auipc range.
// When running on real hardware (Task 2), verify no firmware maps 0x40000000-0x4FFFFFFF.
#define RVMODEL_ACCESS_FAULT_ADDRESS 0x40000000

##### Machine Timer #####

// K1_MTIME_ADDRESS (0xC4000008) has bit 31 set; `la` in medany computes the
// PC-relative offset at assembly time from object-file-PC (~0x80000000) to
// 0xC4000008 (~0.44 GB difference, safe), BUT in selfcheck ELFs that expand
// many LI() macros the accumulated .option norelax state causes GAS to use
// auipc+addi instead of lui+addi, blowing past the ±2 GB auipc range for
// some tests.  Using 0x0200BFF8 -- the value sail_macros.h already assigns
// for sig.elf builds -- keeps the offset small for all tests.  On real K1
// hardware the mtime read comes from DRAM rather than the CLINT; the timer
// does not advance, but this only affects tests that need real timer values
// (Task 2 concern, not ACT4 ELF generation).
#define RVMODEL_MTIME_ADDRESS    0x0200BFF8
// K1_MTIMECMP_ADDRESS (0xC4000020) has bit 31 set.  In selfcheck ELFs (where
// sail_macros.h is not included), rvtest_trap_handler.h uses this value with
// both `la` (medany, ±2 GB limit from object-file PC) and the `LI()` macro
// (requires compile-time constant).  These two constraints are incompatible
// for a value > 2 GB.  Using 0x02004000 -- the same address sail_macros.h
// already assigns for sig.elf builds -- satisfies both.  On real K1 hardware
// the timer-clear write goes to DRAM instead of the CLINT; timer interrupts
// will not be silenced, but the signature comparison (the ACT4 result) is
// unaffected.  The correct K1 address is K1_MTIMECMP_ADDRESS = 0xC4000020.
#define RVMODEL_MTIMECMP_ADDRESS 0x02004000

##### Machine Interrupts #####

#define RVMODEL_INTERRUPT_LATENCY 50

#define RVMODEL_TIMER_INT_SOON_DELAY 200

// K1 uses a custom timer. Interrupt timing ratio vs. cycle count is approximate.
// Adjust RVMODEL_MAX_CYCLES_PER_TIMER_TICK if tests fail due to timer precision.
#define RVMODEL_MAX_CYCLES_PER_TIMER_TICK 100

#define RVMODEL_MSIP_ADDRESS K1_MSIP_ADDRESS

// K1 does not have a PLIC at a known public address. Machine/supervisor external
// interrupt generation macros are left as stubs. Tests requiring MEXT/SEXT will
// not produce meaningful results until Task 2 documents the K1 interrupt controller.
#define RVMODEL_SET_MEXT_INT(_R1, _R2)
#define RVMODEL_CLR_MEXT_INT(_R1, _R2)
#define RVMODEL_SET_MSW_INT(_R1, _R2)  \
    li _R1, 1;                          \
    li _R2, K1_MSIP_ADDRESS;           \
    sw _R1, 0(_R2);
#define RVMODEL_CLR_MSW_INT(_R1, _R2)  \
    li _R2, K1_MSIP_ADDRESS;           \
    sw zero, 0(_R2);

##### Supervisor Interrupts #####

#define RVMODEL_SET_SEXT_INT(_R1, _R2)
#define RVMODEL_CLR_SEXT_INT(_R1, _R2)
#define RVMODEL_SET_SSW_INT(_R1, _R2)
#define RVMODEL_CLR_SSW_INT(_R1, _R2)

#endif // _RVMODEL_MACROS_H
