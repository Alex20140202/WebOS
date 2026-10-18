/* WebOS - 内核主入口：初始化子系统并进入 shell */
#include "kernel.h"

/* shell_run、_kernel_start 声明都在 kernel.h */

/* 引导信息（boot.S 压栈；mbinfo 里含内存映射，后续用） */
void kernel_main(uint32_t magic, uint32_t mbinfo);

void kernel_main(uint32_t magic, uint32_t mbinfo) {
    serial_init();          /* 先于任何输出，让 printk 同时镜像到 COM1 */
    terminal_initialize();

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("WebOS " __TIME__ " " __DATE__ "\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));

    if (magic != 0x2BADB002) {
        printk("warning: multiboot magic = 0x%x (expected 0x2BADB002)\n", magic);
    }

    printk("initializing pmm ... ");
    pmm_init(mbinfo);           /* 解析 multiboot 内存映射 */
    printk("ok (%u MB total, %u MB free)\n",
           pmm_total() / (1024 * 1024), pmm_free() / (1024 * 1024));

    printk("initializing heap ... ");
    heap_init();
    printk("ok\n");

    printk("initializing gdt ... ");
    gdt_init();
    printk("ok\n");

    printk("initializing idt ... ");
    idt_init();
    printk("ok\n");

    printk("initializing paging (identity 16MB) ... ");
    paging_init();
    printk("ok (cr3=%p)\n", (uintptr_t)paging_dir_frame());

    printk("initializing rtc ... ");
    rtc_init();
    printk("ok\n");

    printk("initializing pit (100 Hz) ... ");
    pit_init(100);
    printk("ok\n");

    printk("initializing keyboard ... ");
    keyboard_init();
    printk("ok\n");

    printk("kernel image: %p - %p\n",
           (uintptr_t)_kernel_start, (uintptr_t)_kernel_end);
    printk("ready. type 'help' for commands.\n\n");

    shell_run();
}
