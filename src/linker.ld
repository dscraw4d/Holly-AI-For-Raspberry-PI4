ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
  . = 0x80000;
  .text : { KEEP(*(.text.boot)) *(.text*) } :text
  .rodata : { *(.rodata*) } :text
  . = ALIGN(4096);
  .data : { *(.data*) } :data
  __image_end = .;
  . = ALIGN(8);
  __bss_start = .;
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } :data
  . = ALIGN(8);
  __bss_end = .;
  . = ALIGN(16);
  .stack (NOLOAD) : { . += 262144; } :data
  __stack_top = .;
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
  ASSERT((_start == 0x80000), "kernel entry must match Pi firmware load address")
  ASSERT((__bss_end % 8 == 0), "BSS end must be aligned for startup clearing")
  ASSERT((__stack_top < 0x1000000), "kernel exceeds reserved low-memory budget")
}
