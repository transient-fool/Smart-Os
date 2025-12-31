/* Linker script for Cortex-M3 LM3S6965EVB (256KB Flash, 8KB RAM) */
/* Flash 布局：
 *   0x00000000 - 0x0000FFFF: 代码区域 (64KB)
 *   0x00010000 - 0x0003FFFF: 文件系统区域 (192KB)
 */
MEMORY
{
    CODE (rx) : ORIGIN = 0x00000000, LENGTH = 0x00010000
    DATA (rw) : ORIGIN = 0x20000000, LENGTH = 0x00002000
}
ENTRY(Reset_Handler)

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > CODE

    .data : AT (_etext)
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } > DATA

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > DATA

    /* 栈顶 */
    _estack = ORIGIN(DATA) + LENGTH(DATA);
}
