/* WebOS - IDT：32 个 CPU 异常 + 16 个 IRQ + int 0x80 系统调用门 */
#include "kernel.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  always0;
    uint8_t  flags;         /* 0x8E: present, ring0, 32-bit interrupt gate */
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IRQ0_VECTOR 32
#define IRQ1_VECTOR 33

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

/* IRQ 触发计数 */
static uint32_t irq_counts[16];
static uint32_t irq_total_count;

static void idt_load(uint32_t idt_address) {
    __asm__ volatile("lidt (%0)" : : "r"(idt_address) : "memory");
}

/* isr.S 生成的 48 个桩 */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

/* C 侧处理函数（regs 指向 isr_common 压栈后的寄存器帧） */
void isr_handler(uint32_t* regs);
void irq_handler(uint32_t* regs);
void syscall_handler(uint32_t* regs);

/* 寄存器帧在栈上的布局（与 isr.S 的 push 顺序严格对应）
   esp+0  gs, +4 fs, +8 es, +12 ds,
   +16 edi, +20 esi, +24 ebp, +28 保留的 esp,
   +32 ebx, +36 edx, +40 ecx, +44 eax,
   +48 vector, +52 error_code, +56 返回地址, +60 原始 eax ... */
#define REG_EDI(reg)  (*(uint32_t*)((uint32_t)(reg) + 16))
#define REG_ESI(reg)  (*(uint32_t*)((uint32_t)(reg) + 20))
#define REG_EBP(reg)  (*(uint32_t*)((uint32_t)(reg) + 24))
#define REG_EBX(reg)  (*(uint32_t*)((uint32_t)(reg) + 32))
#define REG_EDX(reg)  (*(uint32_t*)((uint32_t)(reg) + 36))
#define REG_ECX(reg)  (*(uint32_t*)((uint32_t)(reg) + 40))
#define REG_EAX(reg)  (*(uint32_t*)((uint32_t)(reg) + 44))
#define REG_VECTOR(reg) (*(uint32_t*)((uint32_t)(reg) + 48))
#define REG_ERRCODE(reg) (*(uint32_t*)((uint32_t)(reg) + 52))

/* 时钟/键盘回调由各自模块提供 */
extern void pit_tick(void);
extern void keyboard_handle_interrupt(void);

const char* exception_name(uint32_t n) {
    static const char* names[32] = {
        "Divide by Zero", "Debug", "NMI", "Breakpoint", "Overflow",
        "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS",
        "Segment Not Present", "Stack-Segment Fault", "General Protection Fault",
        "Page Fault", "Reserved", "x87 FPU Error", "Alignment Check",
        "Machine Check", "SIMD FP Exception", "Virtualization Exception",
        "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    };
    return (n < 32) ? names[n] : "Unknown";
}

static void idt_set_gate(int num, uint32_t handler, uint16_t sel,
                         uint8_t flags) {
    idt[num].base_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].base_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

/* 供 syscall 等模块安装用户态可达的门 */
void idt_set_gate_user(int num, uint32_t handler, uint8_t dpl) {
    uint8_t dag = (uint8_t)(0xEE & ~(3 << 5));   /* 0xE0 ring0 */
    dag = (uint8_t)(dag | ((dpl & 3) << 5));
    idt_set_gate(num, handler, 0x08, dag);
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;
    memset(idt, 0, sizeof(idt));

    void* stubs[48] = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15,
    };
    for (int i = 0; i < 48; i++)
        idt_set_gate(i, (uint32_t)stubs[i], 0x08, 0x8E);

    idt_load((uint32_t)&idtp);

    syscall_init();     /* 安装 int 0x80 门 */
}

static void panic_dump(uint32_t* regs) {
    printk("\n---- exception ----\n");
    printk("eax=%p ebx=%p ecx=%p edx=%p\n",
           (uintptr_t)REG_EAX(regs), (uintptr_t)REG_EBX(regs),
           (uintptr_t)REG_ECX(regs), (uintptr_t)REG_EDX(regs));
    printk("esi=%p edi=%p ebp=%p eip=%p\n",
           (uintptr_t)REG_ESI(regs), (uintptr_t)REG_EDI(regs),
           (uintptr_t)REG_EBP(regs),
           (uintptr_t)(*(uint32_t*)((uint32_t)regs + 56)));
}

void isr_handler(uint32_t* regs) {
    uint32_t vector = REG_VECTOR(regs);
    uint32_t error_code = REG_ERRCODE(regs);

    if (vector == 14) {
        uint32_t cr2 = cr2_read();
        paging_page_fault(cr2, error_code);
        printk("\n[PAGE FAULT] addr=%p code=%x (%s, %s)\n",
               (uintptr_t)cr2, error_code,
               (error_code & 1) ? "protection" : "not-present",
               (error_code & 2) ? "write" : "read");
        panic_dump(regs);
        printk("System halted.\n");
        for (;;)
            hlt();
    }

    printk("\n[EXCEPTION %u] %s\n",
           vector, exception_name(vector));
    panic_dump(regs);
    printk("System halted.\n");
    for (;;)
        hlt();
}

void irq_handler(uint32_t* regs) {
    uint32_t vector = REG_VECTOR(regs);
    uint32_t irq = vector - IRQ0_VECTOR;

    /* spurious 中断：IRQ7/15 常见，读 ISR 校验 */
    if (irq == 7 || irq == 15) {
        uint8_t isr_reg = inb(irq == 7 ? 0x20 : 0xA0);
        if (!(isr_reg & 0x80)) {
            /* spurious：只回主 PIC 的 EOI，不调用处理器 */
            pic_send_eoi(0);
            return;
        }
    }

    irq_counts[irq % 16]++;
    irq_total_count++;

    /* 发到从 PIC 的 IRQ(8-15) 需要 EOI 到两个 PIC */
    if (irq >= 8)
        pic_send_eoi(8);
    pic_send_eoi(0);

    if (vector == IRQ0_VECTOR)
        pit_tick();
    else if (vector == IRQ1_VECTOR)
        keyboard_handle_interrupt();
    /* 其余 IRQ 目前无驱动，仅计数 */
}

uint32_t irq_count(uint8_t irq) {
    return (irq < 16) ? irq_counts[irq] : 0;
}

uint32_t irq_total(void) {
    return irq_total_count;
}
