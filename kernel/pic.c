/* WebOS - 8259A PIC：重映射 IRQ0-15 到中断向量 32-47 */
#include "kernel.h"

#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1
#define PIC_EOI  0x20

void pic_remap(uint8_t offset1, uint8_t offset2) {
    outb(PIC1_CMD, 0x11);   /* ICW1: 初始化 + ICUW4 */
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();
    outb(PIC1_DAT, offset1);/* ICW2: 向量偏移 */
    io_wait();
    outb(PIC2_DAT, offset2);
    io_wait();
    outb(PIC1_DAT, 0x04);   /* ICW3: 从 PIC 接在主 PIC IRQ2 */
    io_wait();
    outb(PIC2_DAT, 0x02);
    io_wait();
    outb(PIC1_DAT, 0x01);   /* ICW4: 8086 模式 */
    io_wait();
    outb(PIC1_DAT, 0x00);   /* 打开全部 IRQ */
    outb(PIC2_DAT, 0x00);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}
