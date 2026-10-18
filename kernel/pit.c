/* WebOS - 8253/8254 PIT 时钟节拍 */
#include "kernel.h"

#define PIT_CMD  0x43
#define PIT_CH0  0x40

static volatile uint32_t tick_count;
static uint32_t tick_freq;

void pit_init(uint32_t freq) {
    if (freq == 0)
        freq = 100;
    tick_freq = freq;
    uint32_t divisor = 1193182 / freq;
    outb(PIT_CMD, 0x36);                    /* ch0, 低/高字节, 方波模式 3 */
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    pic_remap(32, 40);                      /* 重映射并打开 IRQ0 */
    sti();
}

void pit_tick(void) {
    tick_count++;
}

uint32_t pit_ticks(void) {
    return tick_count;
}

uint32_t pit_freq(void) {
    return tick_freq;
}

void pit_sleep_ms(uint32_t ms) {
    uint32_t wait = ms * pit_freq() / 1000;
    if (wait == 0)
        wait = 1;
    uint32_t target = pit_ticks() + wait;
    while (pit_ticks() < target)
        hlt();
}
