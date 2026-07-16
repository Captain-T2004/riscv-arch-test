/*
 * Linker script for SpacemiT K1 / Milk-V Jupiter bare-metal ACT tests.
 *
 * Load address: 0xC8000000 (128 MiB into K1 DRAM, which starts at 0xC0000000).
 *
 * Layout rationale:
 *   0xC0000000 — ACT runner firmware (BOARD_FW_LOAD_ADDR)
 *   0xC1008000 — tohost polling address (BOARD_FIXED_TOHOST_ADDR)
 *   0xC8000000 — test ELF pack + individual ELF load point (here)
 *
 * From 0xC8000000, all K1 peripherals (UART 0xD4017000, CLINT 0xE4000000,
 * PLIC 0xE0000000) are within +-2GB, satisfying the medany code model.
 *
 * Sail simulation: sail.json DRAM-high region covers 0x80000000-0xBFFFFFFF
 * which does NOT include 0xC8000000. Update sail.json memory regions if
 * running Sail simulation against this link address.
 */

OUTPUT_ARCH( "riscv" )
ENTRY(rvtest_entry_point)

SECTIONS
{
  . = 0xC8000000;
  .text.init    : { *(.text.init) }
  .text.rvtest  : { *(.text.rvtest) *(.text.rvtest.*) }
  . = ALIGN(0x4000);
  .data         : { *(.data) }
  . = ALIGN(0x1000);
  .text.rvmodel : { *(.text.rvmodel) *(.text.rvmodel.*) *(.text) *(.text.*) }
  . = ALIGN(0x1000);
  _end = .;
}
