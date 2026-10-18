/* WebOS - CPUID 封装：vendor / brand / 特性位 */
#include "kernel.h"

struct cpuid_out {
    uint32_t a, b, c, d;
};

static void cpuid_leaf(uint32_t leaf, struct cpuid_out* o) {
    __asm__ volatile("cpuid"
                     : "=a"(o->a), "=b"(o->b), "=c"(o->c), "=d"(o->d)
                     : "a"(leaf), "c"(0));
}

void cpuinfo_vendor(char out[13]) {
    struct cpuid_out o;
    cpuid_leaf(0, &o);
    out[0] = (char)(o.b & 0xFF);
    out[1] = (char)((o.b >> 8) & 0xFF);
    out[2] = (char)((o.b >> 16) & 0xFF);
    out[3] = (char)((o.b >> 24) & 0xFF);
    out[4] = (char)(o.d & 0xFF);
    out[5] = (char)((o.d >> 8) & 0xFF);
    out[6] = (char)((o.d >> 16) & 0xFF);
    out[7] = (char)((o.d >> 24) & 0xFF);
    out[8] = (char)(o.c & 0xFF);
    out[9] = (char)((o.c >> 8) & 0xFF);
    out[10] = (char)((o.c >> 16) & 0xFF);
    out[11] = (char)((o.c >> 24) & 0xFF);
    out[12] = 0;
}

uint32_t cpuinfo_features(void) {
    struct cpuid_out o;
    cpuid_leaf(1, &o);
    return o.d;
}

static void print_flag(uint32_t feat, int bit, const char* name) {
    if (feat & (1u << bit))
        printk(" %s", name);
}

void cpuinfo_print(void) {
    char vendor[13];
    cpuinfo_vendor(vendor);
    printk("CPU vendor: %s\n", vendor);

    /* brand string（若支持 leaf 0x80000002..4） */
    struct cpuid_out o;
    cpuid_leaf(0x80000000, &o);
    if (o.a >= 0x80000004u) {
        char brand[49];
        uint32_t* p = (uint32_t*)brand;
        for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u; leaf++) {
            cpuid_leaf(leaf, &o);
            *p++ = o.a;
            *p++ = o.b;
            *p++ = o.c;
            *p++ = o.d;
        }
        brand[48] = 0;
        printk("CPU brand:  %s\n", brand);
    }

    uint32_t f = cpuinfo_features();
    printk("CPU features:");
    print_flag(f, 0,  "fpu");
    print_flag(f, 3,  "pse");
    print_flag(f, 4,  "tsc");
    print_flag(f, 5,  "msr");
    print_flag(f, 6,  "pae");
    print_flag(f, 9,  "apic");
    print_flag(f, 13, "pge");
    print_flag(f, 16, "pat");
    print_flag(f, 19, "clflush");
    print_flag(f, 23, "mmx");
    print_flag(f, 25, "sse");
    print_flag(f, 26, "sse2");
    printk("\n");
}
