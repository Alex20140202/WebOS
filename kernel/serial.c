/* WebOS - COM1 串口：16550A 轮询驱动，内核控制台镜像 */
#include "kernel.h"

#define COM1    0x3F8
#define DATA    0
#define IER     1
#define FCR     2
#define LCR     3
#define MCR     4
#define LSR     5

static int serial_ready;

void serial_init(void) {
    outb((uint16_t)(COM1 + IER), 0x00);     /* 关中断，轮询 */
    outb((uint16_t)(COM1 + LCR), 0x80);     /* DLAB=1 设波特率 */
    outb((uint16_t)(COM1 + DATA), 0x01);    /* 115200 */
    outb((uint16_t)(COM1 + IER), 0x00);
    outb((uint16_t)(COM1 + LCR), 0x03);     /* 8N1 */
    outb((uint16_t)(COM1 + FCR), 0xC7);     /* FIFO 使能+清空 */
    outb((uint16_t)(COM1 + MCR), 0x0B);     /* DTR/RTS/OUT2 */
    serial_ready = 1;
}

void serial_putchar(char c) {
    if (!serial_ready)
        return;
    if (c == '\n')
        serial_putchar('\r');
    while (!(inb((uint16_t)(COM1 + LSR)) & 0x20))
        ;                       /* 等发送保持寄存器空 */
    outb((uint16_t)(COM1 + DATA), (uint8_t)c);
}

void serial_writestring(const char* s) {
    while (*s)
        serial_putchar(*s++);
}

int serial_has_key(void) {
    return serial_ready && (inb((uint16_t)(COM1 + LSR)) & 0x01);
}

int serial_getkey(void) {
    if (!serial_has_key())
        return KEY_NONE;
    char c = (char)inb((uint16_t)(COM1 + DATA));
    if (c == '\r')
        c = '\n';
    return (int)(unsigned char)c;
}
