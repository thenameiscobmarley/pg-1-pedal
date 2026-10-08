/* everything in one RAM region (the emulator doesn't model memory speed; the pedal runs from AXI SRAM + caches) */
ENTRY(_start)
MEMORY { RAM (rwx) : ORIGIN = 0x24000000, LENGTH = 8M }
SECTIONS
{
  .text : { *(.text*) *(.rodata*) *(.glue_7*) *(.ARM.extab*) *(.init*) *(.fini*) } > RAM
  .ARM.exidx : { *(.ARM.exidx*) } > RAM
  .init_array : { *(.init_array*) } > RAM
  .data : { *(.data*) } > RAM
  .bss (NOLOAD) : { __bss_start__ = .; *(.bss*) *(COMMON) *(.sdram_bss*) __bss_end__ = .; } > RAM
  end = .; _end = .;
}
