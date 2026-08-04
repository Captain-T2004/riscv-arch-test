// rvmodel_macros.h
// DUT-specific macro definitions for SpacemiT K1 / Banana Pi BPI-F3
// SPDX-License-Identifier: BSD-3-Clause
//
// Halt mechanism: tohost-based pass/fail signaling (write 1=PASS, 3=FAIL),
// then WFI spin. tohost is a linked symbol (RVMODEL_DATA_SECTION), found by
// the firmware runner via ELF symbol lookup; BOARD_FIXED_TOHOST_ADDR is only
// a fallback if that lookup ever fails.
//
// Hardware addresses (SpacemiT K1, from Bianbu DTS / U-Boot logs):
//   DRAM:                        0x0-0x80000000 (2GiB)
//   UART0 (NS16550-compatible): 0xD4017000
//   CLINT base:                 0xE4000000  (spacemit,k1-clint)
//     MSIP  (hart 0):           0xE4000000
//     MTIME:                    0xE400BFF8
//     MTIMECMP (hart 0):        0xE4004000
//   PLIC:                       0xE0000000  (spacemit,k1-plic)
//
// Memory layout for ACT runner (Task 2):
//   Firmware load+entry: 0x04000000  (BOARD_FW_LOAD_ADDR)
//   tohost:              linked symbol inside each test ELF (see below);
//                        0x04100000 (BOARD_FIXED_TOHOST_ADDR) fallback only
//   Test ELF pack:       0x0C200000  (BOARD_EXT_PACK_ADDR)
//   Test ELF load:       0x04200000  (link.ld ". = 0x04200000")

#ifndef _RVMODEL_MACROS_H
#define _RVMODEL_MACROS_H

// ── Address loading helper ───────────────────────────────────────────────────
// All K1 peripheral addresses have bit 31 set (> 0x80000000), so a plain `li`
// on RV64 sign-extends them to 0xFFFFFFFF_xxxxxxxx — the wrong physical address.
// LOAD_ADDR32 zero-extends a 32-bit address into a 64-bit register via
// slli+srli, which clears the upper 32 bits without a scratch register.
#define LOAD_ADDR32(_REG, _ADDR)               \
    li   _REG, _ADDR                          ;\
    slli _REG, _REG, 32                       ;\
    srli _REG, _REG, 32

// ── K1 UART0 (NS16550 compatible) ───────────────────────────────────────────
#define K1_UART0_BASE    0xD4017000
#define K1_UART_THR_OFF  0x00   /* Transmit Holding Register */
#define K1_UART_LSR_OFF  0x14   /* Line Status Register      */
#define K1_UART_LSR_THRE 0x20   /* Bit 5: TX Holding Empty   */

// ── K1 CLINT ────────────────────────────────────────────────────────────────
#define K1_MSIP_ADDRESS     0xE4000000
#define K1_MTIME_ADDRESS    0xE400BFF8
#define K1_MTIMECMP_ADDRESS 0xE4004000

// ── tohost address (must match BOARD_FIXED_TOHOST_ADDR in bpif3_k1.env) ────
// BOARD_FIXED_TOHOST_ADDR is injected as a compile-time -D flag by the runner
// build system. Default here matches the proposed K1 env file value.
#ifndef BOARD_FIXED_TOHOST_ADDR
#define BOARD_FIXED_TOHOST_ADDR 0x04100000
#endif

#define RVMODEL_DATA_SECTION \
    .pushsection .data,"aw",@progbits;  \
    .align 3;                            \
    .global tohost;                      \
tohost:                                  \
    .dword 0;                            \
    .popsection

#define STANDARD_SM_SUPPORTED

##### STARTUP #####

//#define RVMODEL_BOOT

//#define RVMODEL_BOOT_TO_MMODE

##### TERMINATION #####

// Write 1 (HTIF PASS) to tohost, then spin in WFI.
// The runner monitor (runner_monitor.c:620) checks: if (v == 1) → PASS.
#define RVMODEL_HALT_PASS                              \
    la   a0, tohost                                   ;\
    li   a1, 1                                        ;\
    sd   a1, 0(a0)                                    ;\
    _rvmodel_halt_pass_loop:                          ;\
    wfi                                               ;\
    j    _rvmodel_halt_pass_loop

#define RVMODEL_HALT_FAIL                              \
    la   a0, tohost                                   ;\
    li   a1, 3                                        ;\
    sd   a1, 0(a0)                                    ;\
    _rvmodel_halt_fail_loop:                          ;\
    wfi                                               ;\
    j    _rvmodel_halt_fail_loop

##### IO #####

// UART0 is initialised by the FSBL/SPL and then again by the ACT runner
// firmware before any test ELF runs. RVMODEL_IO_INIT writes LCR=0x03 (8N1)
// as a lightweight re-init guard in case a test ELF is run standalone.
// LOAD_ADDR32 is required because 0xD4017000 has bit 31 set.
#define RVMODEL_IO_INIT(_R1, _R2, _R3)                \
    LOAD_ADDR32(_R1, K1_UART0_BASE)                  ;\
    li   _R2, 0x03                                   ;\
    sb   _R2, 0x0C(_R1)                              ; /* LCR: 8N1 */

#define RVMODEL_IO_WRITE_STR(_R1, _R2, _R3, _STR_PTR) \
1:                                                    ;\
    lbu  _R1, 0(_STR_PTR)                            ;\
    beqz _R1, 3f                                     ;\
2:                                                    ;\
    LOAD_ADDR32(_R2, K1_UART0_BASE)                  ;\
    addi _R2, _R2, K1_UART_LSR_OFF                  ;\
4:                                                    ;\
    lbu  _R3, 0(_R2)                                 ;\
    andi _R3, _R3, K1_UART_LSR_THRE                 ;\
    beqz _R3, 4b                                     ;\
    LOAD_ADDR32(_R2, K1_UART0_BASE)                  ;\
    sb   _R1, K1_UART_THR_OFF(_R2)                  ;\
    addi _STR_PTR, _STR_PTR, 1                       ;\
    j    1b                                          ;\
3:

##### Access Fault #####

// 0x0, matching every other DUT config in this repo (sail/spike/imperas/
// whisper/qemu/cvw). sail.json models 0x0-0xFFF as an intentional unmapped
// null-guard for exactly this macro. Real K1 DRAM technically starts at
// physical 0x0 per the vendor DTS, so this is the standard ACT reference-model
// convention, not a claim about real hardware behavior at address 0.
#define RVMODEL_ACCESS_FAULT_ADDRESS 0x00000000

##### Machine Timer #####

// RVMODEL_MTIMECMP_ADDRESS is defined as a relocatable linker symbol
// (_k1_mtimecmp) rather than a numeric constant. This allows la/LA to emit
// auipc+addi (pc-relative) from the ELF load base (currently 0x04200000),
// avoiding the RV64 sign-extension failure (lui+addi cannot represent
// 0xE400xxxx correctly: bit 31 = 1). It is only ever read through the
// RVMODEL_LOAD_MTIMECMP_ADDR hook below (LOAD_ADDR32, absolute), never a
// bare la/auipc, so the >2GB pc-relative distance to the real CLINT address
// never actually matters.
// The symbol is placed via a zero-size NOLOAD section in link.ld.
// No declaration needed in assembly — linker globals are visible by name.
//
// RVMODEL_MTIME_ADDRESS is intentionally left undefined: it has no
// equivalent LOAD-hook indirection anywhere it's consumed (interrupts.py,
// Sm.py's cp_mtime_write), so any generated test that uses it emits a bare
// LA(reg, RVMODEL_MTIME_ADDRESS) straight to the real MTIME MMIO address
// (0xE400BFF8) — a pc-relative auipc target ~3.79GB from our 0x04200000
// test link base, which exceeds auipc's +-2GB reach and fails to assemble
// ("offset too large"). Leaving it undefined means "no machine timer
// interrupts are tested" (see tests/env/check_defines.h) — a real, narrow
// coverage gap, not a workaround; only relevant to Interrupts* tests
// (already excluded) and Sm_mcsr_cntr-00's cp_mtime_write coverpoint.
#define RVMODEL_MTIMECMP_ADDRESS _k1_mtimecmp

// RVMODEL_LOAD_MTIMECMP_ADDR: board-specific hook checked by
// rvtest_trap_handler.h before falling back to LI(reg, RVMODEL_MTIMECMP_ADDRESS).
#define RVMODEL_LOAD_MTIMECMP_ADDR(_REG) LOAD_ADDR32(_REG, K1_MTIMECMP_ADDRESS)

##### Machine Interrupts #####

#define RVMODEL_INTERRUPT_LATENCY 50

#define RVMODEL_TIMER_INT_SOON_DELAY 200

#define RVMODEL_MAX_CYCLES_PER_TIMER_TICK 100

#define RVMODEL_MSIP_ADDRESS K1_MSIP_ADDRESS

// K1 PLIC is at 0xE0000000. External interrupt generation requires writing
// to PLIC priority/threshold registers. Left as stubs until the exact PLIC
// register layout is confirmed from K1 hardware testing.
#define RVMODEL_SET_MEXT_INT(_R1, _R2)
#define RVMODEL_CLR_MEXT_INT(_R1, _R2)

#define RVMODEL_SET_MSW_INT(_R1, _R2)  \
    LOAD_ADDR32(_R2, K1_MSIP_ADDRESS) ;\
    li _R1, 1                         ;\
    sw _R1, 0(_R2);

#define RVMODEL_CLR_MSW_INT(_R1, _R2)  \
    LOAD_ADDR32(_R2, K1_MSIP_ADDRESS) ;\
    sw zero, 0(_R2);

##### Supervisor Interrupts #####

#define RVMODEL_SET_SEXT_INT(_R1, _R2)
#define RVMODEL_CLR_SEXT_INT(_R1, _R2)
#define RVMODEL_SET_SSW_INT(_R1, _R2)
#define RVMODEL_CLR_SSW_INT(_R1, _R2)

#endif // _RVMODEL_MACROS_H
