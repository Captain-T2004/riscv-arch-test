/*
 * Linker script for SpacemiT K1 / Milk-V Jupiter bare-metal ACT tests.
 *
 * Load address: 0x80000000 (2GB into K1 DRAM).
 *
 * This address is chosen for Sail simulation and JTAG-based loading only.
 * It keeps all K1 peripheral addresses (0xC0000000-0xDFFFFFFF) within
 * +-2GB of the PC, which the medany code model requires.
 *
 * WARNING: 0x80000000 is also where OpenSBI loads on most RISC-V platforms.
 * If Task 2 uses U-Boot or any resident firmware to load the ELF, this
 * address will conflict. In that case, change to an address above the
 * firmware footprint (e.g. 0xA0000000) and update sail.json memory regions
 * to match. JTAG-based loading (no resident firmware) can use 0x80000000.
 */

OUTPUT_ARCH( "riscv" )
ENTRY(rvtest_entry_point)

SECTIONS
{
  . = 0x80000000;
  .text.init    : { *(.text.init) }
  .text.rvtest  : { *(.text.rvtest) *(.text.rvtest.*) }
  . = ALIGN(0x4000);
  .data         : { *(.data) }
  . = ALIGN(0x1000);
  .text.rvmodel : { *(.text.rvmodel) *(.text.rvmodel.*) *(.text) *(.text.*) }
  . = ALIGN(0x1000);
  _end = .;
}
