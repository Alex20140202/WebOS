/* WebOS - VGA 文本模式终端：带光标管理、滚动 */
#include "kernel.h"

static uint16_t* const terminal_buffer = (uint16_t*)VGA_ADDRESS;
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static int serial_mirror = 1;   /* 输出镜像到 COM1 */

/* serial.c 提供；未就绪时内部为 no-op */
extern void serial_putchar(char c);

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_clear();
}

void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    terminal_row = 0;
    terminal_column = 0;
    terminal_set_cursor(0, 0);
}

void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_set_cursor(size_t row, size_t col) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        memcpy(&terminal_buffer[(y - 1) * VGA_WIDTH],
               &terminal_buffer[y * VGA_WIDTH],
               VGA_WIDTH * sizeof(uint16_t));
    for (size_t x = 0; x < VGA_WIDTH; x++)
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            vga_entry(' ', terminal_color);
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    terminal_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

void terminal_putchar(char c) {
    if (serial_mirror && (c >= 0x20 || c == '\n'))
        serial_putchar(c);
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_scroll();
    } else if (c == '\r') {
        terminal_column = 0;
    } else if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        }
    } else if (c == '\t') {
        do {
            terminal_putchar(' ');
        } while (terminal_column % 4 != 0);
    } else {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT)
                terminal_scroll();
        }
    }
    terminal_set_cursor(terminal_row, terminal_column);
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

void terminal_get_cursor(size_t* row, size_t* col) {
    outb(0x3D4, 0x0F);
    uint16_t lo = inb(0x3D5);
    outb(0x3D4, 0x0E);
    uint16_t hi = inb(0x3D5);
    uint16_t pos = (uint16_t)(lo | (hi << 8));
    *row = pos / VGA_WIDTH;
    *col = pos % VGA_WIDTH;
}

void terminal_erase_line(void) {
    for (size_t x = 0; x < VGA_WIDTH; x++)
        terminal_putentryat(' ', terminal_color, x, terminal_row);
    terminal_column = 0;
    terminal_set_cursor(terminal_row, terminal_column);
}

void terminal_erase_to_end(void) {
    for (size_t x = terminal_column; x < VGA_WIDTH; x++)
        terminal_putentryat(' ', terminal_color, x, terminal_row);
    terminal_set_cursor(terminal_row, terminal_column);
}
