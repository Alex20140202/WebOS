/* WebOS - 系统调用：int 0x80 门（DPL3）+ 分发 */
#include "kernel.h"

/* regs 指向 isr.S syscall_stub 保存的寄存器区，布局与 isr_common 相同。
   regs[8]=ebx, regs[9]=edx, regs[10]=ecx, regs[11]=eax */
void syscall_handler(uint32_t* regs) {
    uint32_t num  = regs[11];   /* 原始 eax = 系统调用号 */
    uint32_t a    = regs[8];    /* ebx */
    uint32_t b    = regs[10];   /* ecx */
    uint32_t c    = regs[9];    /* edx */
    int32_t ret = syscall_dispatch(num, a, b, c);
    regs[11] = (uint32_t)ret;   /* 返回值写回 eax */
}

int32_t syscall_dispatch(uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    switch (num) {
    case SYSCALL_WRITE: {
        if (a != 1 || !b)
            return -1;
        uint32_t len = c;
        if (len > 4096)
            len = 4096;         /* 防御性长度上限 */
        terminal_write((const char*)b, len);
        return (int32_t)len;
    }
    case SYSCALL_EXIT:
        printk("[exit] syscall code %u: task suspended\n", a);
        for (;;)
            hlt();
        return 0;
    case SYSCALL_UPTIME:
        return (int32_t)pit_ticks();
    case SYSCALL_SLEEP:
        pit_sleep_ms(a);
        return 0;
    case SYSCALL_MEMINFO: {
        if (!b)
            return -1;
        uint32_t* out = (uint32_t*)b;
        out[0] = pmm_total();
        out[1] = pmm_free();
        out[2] = heap_total();
        out[3] = heap_used_bytes();
        return 0;
    }
    default:
        return -1;
    }
}

void syscall_init(void) {
    extern void idt_set_gate_user(int num, uint32_t handler, uint8_t dpl);
    idt_set_gate_user(128, (uint32_t)syscall_stub, 3);
}
