/* WebOS 引导代码 —— Multiboot(spec 0.6.96) 兼容
   GRUB 会加载此 ELF 内核并跳到 _start，此时 CPU 已进入保护模式，
   不需要自己设置段描述符（GDT 已由 GRUB 设置好）。 */

.set MB_MAGIC, 0x1BADB002
.set MB_FLAGS, 0x00000003   /* 0x1|0x2: 需要内存信息 + 保持对齐 */
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

.section .bss, "aw", @nobits
.align 16
stack_bottom:
.skip 16384           /* 16 KiB 内核栈 */
stack_top:

.section .text
.globl _start
_start:
    movl $stack_top, %esp
    pushl %ebx          /* Multiboot 信息结构体物理地址（GRUB 填充） */
    pushl %eax          /* 魔数 0x2BADB002，校验引导程序 */
    cld

    call kernel_main

1:  hlt
    jmp 1b

.size _start, . - _start
