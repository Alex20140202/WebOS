/* WebOS - 通过 8042 键盘控制器重启 */
#include "kernel.h"

void shutdown(void) {
    printk("Shutting down.\n");
    /* QEMU（新---） ACPI PM1a 端口，再尝试 Bochs/老 QEMU */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    for (;;)
        hlt();
}

void reboot(void) {
    /* 等待输入缓冲空 */
    for (int i = 0; i < 100000 && (inb(0x64) & 0x02); i++)
        ;
    cli();
    outb(0x64, 0xFE);       /* 8042 pulse line 0: 复位 CPU */
    for (;;)
        hlt();
}
