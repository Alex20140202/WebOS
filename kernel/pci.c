/* WebOS - PCI 配置空间枚举 */
#include "kernel.h"

#define PCI_ADDR_PORT 0xCF8
#define PCI_DATA_PORT 0xCFC

#define PCI_CLASS(c)      ((c >> 16) & 0xFF)
#define PCI_SUBCLASS(c)   ((c >> 8) & 0xFF)
#define PCI_MULTIFUNC(h)  ((h & 0x800000u) != 0)

static int pci_absent(uint32_t id) {
    return id == 0xFFFFFFFFu || (id & 0xFFFF) == 0xFFFFu;
}

uint32_t pci_config_read(uint32_t bus, uint32_t slot, uint32_t func,
                         uint8_t offset) {
    uint32_t addr = (1u << 31)                     /* enable 位 */
                  | (bus << 16) | (slot << 11) | (func << 8)
                  | (offset & 0xFC);
    /* CONFIG_ADDRESS 必须整 32 位一次写出，逐字节写不会被硬件采纳 */
    __asm__ volatile("outl %0, %1"
                     : : "a"(addr), "Nd"(PCI_ADDR_PORT) : "memory");

    uint32_t d = 0;
    for (uint8_t i = 0; i < 4; i++)
        d |= (uint32_t)inb((uint16_t)(PCI_DATA_PORT + i)) << (i * 8);
    return d;
}

static const char* class_name(uint32_t cls, uint32_t sub) {
    switch (cls) {
    case 0x00: return "old device";
    case 0x01:
        if (sub == 0x06) return "SATA ctrl";
        if (sub == 0x08) return "NVME ctrl";
        return "IDE ctrl";
    case 0x02: return "network";
    case 0x03:
        if (sub == 0x00) return "VGA ctrl";
        return "display";
    case 0x04: return "multimedia";
    case 0x05: return "memory ctrl";
    case 0x06:
        if (sub == 0x00) return "host bridge";
        if (sub == 0x01) return "ISA bridge";
        if (sub == 0x04) return "PCI-to-PCI bridge";
        return "bridge";
    case 0x07: return "serial ctrl";
    case 0x08:
        if (sub == 0x00) return "PIC";
        if (sub == 0x01) return "DMA ctrl";
        if (sub == 0x03) return "timer";
        return "system peripheral";
    case 0x09: return "input";
    case 0x0C:
        if (sub == 0x03) return "USB ctrl";
        return "serial bus";
    case 0x0D: return "wireless";
    default:   return "unknown";
    }
}

void pci_scan(void) {
    printk("PCI devices:\n");
    printk("  bus.slot.func vendor:device class    [desc]\n");

    uint32_t found = 0;
    for (uint32_t bus = 0; bus < 8; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            /* 0x0C 处是多功能设备的 header type，先用它筛掉空槽 */
            uint32_t hdr = pci_config_read(bus, slot, 0, 0x0C);
            if (pci_absent(hdr))
                continue;

            uint32_t func_max = PCI_MULTIFUNC(hdr) ? 8 : 1;
            for (uint32_t func = 0; func < func_max; func++) {
                /* offset 0 的 dword：低 16 位 vendor，高 16 位 device */
                uint32_t id = pci_config_read(bus, slot, func, 0);
                if (pci_absent(id))
                    continue;

                uint32_t vendor = id & 0xFFFF;
                uint32_t device = id >> 16;
                uint32_t cls_reg = pci_config_read(bus, slot, func, 0x08);
                uint32_t cls  = PCI_CLASS(cls_reg);
                uint32_t sub  = PCI_SUBCLASS(cls_reg);

                printk("  %u.%u.%u  %04x:%04x  %02x %02x  [%s]\n",
                       bus, slot, func, vendor, device,
                       cls, sub, class_name(cls, sub));
                found++;
            }
        }
    }
    if (!found)
        printk("  none found\n");
    else
        printk("  total: %u device(s)\n", found);
}