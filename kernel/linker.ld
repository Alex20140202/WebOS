/* WebOS 链接脚本 —— Multiboot ELF 内核 */
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    /* 内核加载在 1MB（GRUB 标准） */
    . = 0x100000;

    _kernel_start = .;

    .multiboot : {
        *(.multiboot)
    } :text

    .text : {
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    .data : {
        *(.data*)
    } :data

    .bss : {
        *(.bss*)
        *(COMMON)
    } :data

    _kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}

/* RWX LOAD 分段消警告：代码段 RX，数据段 RW */
PHDRS
{
    text  PT_LOAD FLAGS(5);   /* R-X */
    data  PT_LOAD FLAGS(6);   /* RW- */
}

