/* WebOS - PCI 配置空间枚举 */
#include "kernel.h"

#define PCI_ADDR_PORT 0xCF8
#define PCI_DATA_PORT 0xCFC

#define PCI_CLASS(c)      ((c >> 16) & 0xFF)
#define PCI_SUBCLASS(c)   ((c >> 8) & 0xFF)
#define PCI_MULTIFUNC(h)  ((h & 0x800000u) != 0)

uint32_t pci_config_read(uint32_t bus, uint32_t slot, uint32_t func,
                         uint8_t offset) {
    uint32_t addr = (1u << 31)          /* enable 位 */
                  | (bus << 16) | (slot << 11) | (func << 8)
                  | (offset & 0xFC);
    outb(PCI_ADDR_PORT, (uint8_t)(addr & 0xFF));
    outb((uint16_t)(PCI_ADDR_PORT + 1), (uint8_t)((addr >> 8) & 0xFF));
    outb((uint16_t)(PCI_ADDR_PORT + 2), (uint8_t)((addr >> 16) & 0xFF));
    outb((uint16_t)(PCI_ADDR_PORT + 3), (uint8_t)((addr >> 24) & 0xFF));
    return inw(PCI_DATA_PORT) | (inw((uint16_t)(PCI_DATA_PORT + 2)) << 16);
}

static const char* class_name(uint32_t class, uint32_t subclass) {
    switch (class) {
    case 0x00: return "old device";
    case 0x01:
        if (subclass == 0x06) return "SATA ctrl";
        return "IDE ctrl";
    case 0x02: return "network";
    case 0x03:
        if (subclass == 0x00) return "VGA ctrl";
        if (subclass == 0x80) return "VGA ctrl (misc)";
        return "display";
    case 0x04: return "multimedia";
    case 0x05: return "memory ctrl";
    case 0x06:
        if (subclass == 0x00) return "host bridge";
        if (subclass == 0x01) return "ISA bridge";
        if (subclass == 0x04) return "PCI-to-PCI";
        return "bridge";
    case 0x07: return "serial ctrl";
    case 0x0C:
        if (subclass == 0x03) return "USB ctrl";
        return "serial bus";
    default:   return "unknown";
    }
}

void pci_scan(void) {
    printk("PCI devices:\n");
    printk("  bus.slot.func vendor:device class    [desc]\n");
    uint32_t found = 0;
    for (uint32_t bus = 0; bus < 8; bus++) {
        /* 一次读出 suas configuration 空间的 header */
        for (uint32_t slot = 0; slot < 32; slot++) {
            uint32_t vend = pci_config_read(bus, slot, 0, 0);
            if (vend == 0xFFFFFFFFu)
                continue;
            uint32_t func_max = 1;
            uint32_t hdr = pci_config_read(bus, slot, 0, 0x0C);
            if (PCI_MULTIFUNC(hdr))
                func_max = 8;
            for (uint32_t func = 0; func < func_max; func++) {
                vend = pci_config_read(bus, slot, func, 0);
                if (vend == 0xFFFFFFFFu)
                    continue;
                uint32_t dev = pci_config_read(bus, slot, func, 2);
                uint32_t class = pci_config_read(bus, slot, func, 8);
                uint32_t cls = PCI_CLASS(class);
                printk("  %u.%u.%u  %04x:%04x  %02x %02x  [%s]\n",
                       bus, slot, func,
                       vend & 0xFFFF, (dev & 0xFFFF),
                       cls, PCI_SUBCLASS(class),
                       class_name(cls, PCI_SUBCLASS(class)));
                found++;
            }
        }
    }
    if (!found)
        printk("  none found\n");
}
