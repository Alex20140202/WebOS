/* WebOS - gdt_flush.S : 加载 GDT 后重载段寄存器 */
.section .text
.globl gdt_flush
gdt_flush:
    movl 4(%esp), %eax
    lgdt (%eax)

    movw $0x10, %ax        /* 数据段 0x10 */
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ljmp $0x08, $flush2    /* 远跳转载入代码段 0x08 */
flush2:
    ret
