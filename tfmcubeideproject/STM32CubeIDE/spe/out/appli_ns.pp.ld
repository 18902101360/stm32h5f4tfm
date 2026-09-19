;
MEMORY
{
  FLASH (rx) : ORIGIN = 0x080D0400, LENGTH = (((0x12C000) - 0x400 - 0x3000))
  FLASH_DATA (r) : ORIGIN = ((((0x08000000)) + ((((((0x200000)) + ((0x58000))) + ((0x12C000))))))), LENGTH = ((((0x200000)+(0x200000)) - (((((0x200000)) + ((0x58000))) + ((0x12C000))))))
  RAM (rwx) : ORIGIN = ((((0x20000000)) + (0))), LENGTH = ((((0x40000) +(0x20000 - (0x400))) - (((0x20000 - (0x400))) + (0))))
  RAM2 (rwx) : ORIGIN = ((0x20060000)), LENGTH = ((0x60000) + (0x60000) + (0x60000))
}
__heap_size__ = 0x0001000;
__stack_size__ = 0x0001000;
ENTRY(Reset_Handler)
SECTIONS
{
    .text (READONLY) :
    {
        KEEP(*(.vectors))
        __Vectors_End = .;
        __Vectors_Size = __Vectors_End - __Vectors;
        __end__ = .;
        *(.text*)
        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);
        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);
        . = ALIGN(4);
        __copy_table_start__ = .;
        LONG (__etext)
        LONG (__data_start__)
        LONG ((__data_end__ - __data_start__) / 4)
        LONG (DEFINED(__etext2) ? __etext2 : 0)
        LONG (DEFINED(__data2_start__) ? __data2_start__ : 0)
        LONG (DEFINED(__data2_start__) ? ((__data2_end__ - __data2_start__) / 4) : 0)
        __copy_table_end__ = .;
        . = ALIGN(4);
        __zero_table_start__ = .;
        LONG (__bss_start__)
        LONG ((__bss_end__ - __bss_start__) / 4)
        LONG (DEFINED(__bss2_start__) ? __bss2_start__ : 0)
        LONG (DEFINED(__bss2_start__) ? ((__bss2_end__ - __bss2_start__) / 4) : 0)
        __zero_table_end__ = .;
        KEEP(*(.init))
        KEEP(*(.fini))
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)
         *crtbegin.o(.dtors)
         *crtbegin?.o(.dtors)
         *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
         *(SORT(.dtors.*))
         *(.dtors)
        *(.rodata*)
        KEEP(*(.eh_frame*))
    } > FLASH
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH
    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;
    __etext = ALIGN(4);
    .testprotection ((((0x20000000)) + (0))) :
    {
         *(.bss.NoInit);
    } > RAM
    .data (((((0x20000000)) + (0)))+(0x100)) : AT (__etext)
    {
        __data_start__ = .;
        *(vtable)
        *(.data*)
        KEEP(*(.jcr*))
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM
    .bss :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM
    bss_size = __bss_end__ - __bss_start__;
    .stack : ALIGN(32)
    {
        . += __stack_size__;
    } > RAM
    __StackLimit = ADDR(.stack);
    __StackTop = ADDR(.stack) + SIZEOF(.stack);
    .heap : ALIGN(8)
    {
        __end__ = .;
        PROVIDE(end = .);
        __HeapBase = .;
        . += __heap_size__;
        __HeapLimit = .;
        __heap_limit = .;
    } > RAM
    PROVIDE(__stack = __StackTop);
    .ram2 (NOLOAD) :
    {
        __ns_ram2_start__ = .;
        __bss2_start__ = .;
        KEEP(*(.ram2*))
        *(.bss.ram2*)
        . = ALIGN(8);
        __bss2_end__ = .;
        __ns_ram2_heap_start__ = .;
        . = ORIGIN(RAM2) + LENGTH(RAM2);
        __ns_ram2_heap_end__ = .;
        __ns_ram2_end__ = .;
    } > RAM2
    .ns_flash_data (NOLOAD) :
    {
        __ns_flash_data_start__ = .;
        KEEP(*(.ns_flash_data*))
        . = ((((0x200000)+(0x200000)) - (((((0x200000)) + ((0x58000))) + ((0x12C000))))));
        __ns_flash_data_end__ = .;
        __ns_flash_data_size__ = SIZEOF(.ns_flash_data);
    } > FLASH_DATA
}
