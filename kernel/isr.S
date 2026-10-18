/* WebOS - 中断桩：为 32 个异常和 16 个 IRQ 生成 ISR，调用 C 处理函数 */

.section .text

/* 异常（无错误码）：push 0 占位再 push vector */
.macro ISR_NOERR n
.globl isr\n
isr\n:
    pushl $0
    pushl $\n
    jmp isr_common
.endm

/* 异常（带错误码）：CPU 自动 push 错误码 */
.macro ISR_ERR n
.globl isr\n
isr\n:
    pushl $\n
    jmp isr_common
.endm

/* IRQ：push 0 占位再 push 中断向量（\v），\n 只是标签编号 */
.macro IRQ v, n
.globl irq\n
irq\n:
    pushl $0
    pushl $\v
    jmp irq_common
.endm

isr_common:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushl %esp          /* 传寄存器帧给 isr_handler */
    call isr_handler
    addl $4, %esp       /* 丢掉传给处理器的寄存器帧指针 */
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp       /* 丢掉 vector + 错误码占位 */
    iret

irq_common:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushl %esp          /* 传寄存器帧给 irq_handler */
    call irq_handler
    addl $4, %esp
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp
    iret

/* int 0x80 系统调用：与 isr_common 类似，但要把返回值写回保存的 eax */
.globl syscall_stub
syscall_stub:
    pushl $0            /* 占位错误码 */
    pushl $128          /* 向量号 */
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushl %esp          /* 传寄存器区指针 */
    call syscall_handler
    /* 保存区偏移: 4(gsp) + 4(gac) + 4*(gs,fs,es,ds) + 32(pusha 8 寄存器
       里 eax 是最后压入的，位于最高处) */
    movl %eax, 48(%esp) /* 写回保存的 eax 作为返回值 */
    addl $4, %esp
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp
    iret

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

IRQ 32, 0
IRQ 33, 1
IRQ 34, 2
IRQ 35, 3
IRQ 36, 4
IRQ 37, 5
IRQ 38, 6
IRQ 39, 7
IRQ 40, 8
IRQ 41, 9
IRQ 42, 10
IRQ 43, 11
IRQ 44, 12
IRQ 45, 13
IRQ 46, 14
IRQ 47, 15
