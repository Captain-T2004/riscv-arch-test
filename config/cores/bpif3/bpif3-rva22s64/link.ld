/*
 * Linker script for SpacemiT K1 / Banana Pi BPI-F3 bare-metal ACT tests.
 *
 * Load address: 0x04200000.
 *
 * Layout rationale:
 *   0x04000000 — ACT runner firmware (BOARD_FW_LOAD_ADDR)
 *   0x04100000 — tohost polling address (BOARD_FIXED_TOHOST_ADDR)
 *   0x04200000 — test ELF pack + individual ELF load point (here)
 *
 * K1 peripherals (UART 0xD4017000, CLINT 0xE4000000, PLIC 0xE0000000) are
 * more than 2GB from this load address, so they are reached via LOAD_ADDR32
 * (absolute li+shift, see rvmodel_macros.h) rather than PC-relative auipc.
 * Only _k1_mtimecmp below is exposed as a linker symbol, and it is only ever
 * read through the RVMODEL_LOAD_MTIMECMP_ADDR hook, never a bare la/auipc.
 */

OUTPUT_ARCH( "riscv" )
ENTRY(rvtest_entry_point)

SECTIONS
{
  . = 0x04200000;
  .text.init    : { *(.text.init) }
  .text.rvtest  : { *(.text.rvtest) *(.text.rvtest.*) }
  . = ALIGN(0x4000);
  .data         : { *(.data) }
  . = ALIGN(0x1000);
  .text.rvmodel : { *(.text.rvmodel) *(.text.rvmodel.*) *(.text) *(.text.*) }
  . = ALIGN(0x1000);
  _end = .;

  /* Peripheral-address stub: a zero-size NOLOAD section so that
   * RVMODEL_MTIMECMP_ADDRESS expands to a relocatable symbol instead of a
   * numeric constant. GAS then emits auipc+addi (pc-relative) for la/LA,
   * which avoids the RV64 sign-extension problem that lui+addi hits for
   * addresses with bit 31 set. The section holds no bytes; only the
   * symbol VMA matters. */
  .k1_periph_mtimecmp 0xE4004000 (NOLOAD) : { _k1_mtimecmp = .; }
}
