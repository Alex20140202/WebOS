/* WebOS - 交互式 shell：readline（历史+编辑）+ 命令分发 */
#include "kernel.h"

#define SHELL_PROMPT  "> "
#define INPUT_MAX     128
#define HIST_MAX      8

static char input[INPUT_MAX];
static size_t input_len;

static char hist[HIST_MAX][INPUT_MAX];
static int  hist_count;
static int  hist_pos;       /* 浏览位置 */

static void cmd_help(void) {
    printk("WebOS shell commands:\n");
    printk("  help       - show this help\n");
    printk("  clear      - clear the screen\n");
    printk("  echo ...   - print arguments\n");
    printk("  uptime     - seconds since boot\n");
    printk("  mem        - kernel image info\n");
    printk("  memmap     - physical memory usage\n");
    printk("  heap       - kernel heap usage\n");
    printk("  irqstat    - IRQ trigger statistics\n");
    printk("  alloc      - kmalloc/kfree stress test\n");
    printk("  sleep N    - busy-wait N seconds (max 60)\n");
    printk("  date       - show CMOS RTC date/time\n");
    printk("  page A     - translate virtual address A (hex)\n");
    printk("  map A F    - map one page at va A (hex) flags F (u/r)\n");
    printk("  unmap A    - unmap page at va A (hex)\n");
    printk("  pgstat     - paging status and page-fault stats\n");
    printk("  cpuinfo    - CPU vendor/brand/features\n");
    printk("  pci        - enumerate PCI devices\n");
    printk("  serial     - COM1 status\n");
    printk("  syscall    - demo int 0x80 dispatch\n");
    printk("  test       - run kernel selftests\n");
    printk("  about      - about this system\n");
    printk("  history    - show command history\n");
    printk("  reboot     - reset via keyboard controller\n");
    printk("  shutdown   - power off (QEMU/Bochs ACPI)\n");
    printk("  halt       - halt the cpu\n");
    printk("tips: up/down arrows recall history, backspace edits.\n");
}

static void cmd_echo(const char* args) {
    printk("%s\n", args);
}

static void cmd_mem(void) {
    printk("kernel image: %p - %p (%u KB)\n",
           (uintptr_t)_kernel_start,
           (uintptr_t)_kernel_end,
           ((_kernel_end - _kernel_start) + 1023) / 1024);
}

static void cmd_memmap(void) {
    printk("physical memory: %u KB total, %u KB free, page size %u\n",
           pmm_total() / 1024, pmm_free() / 1024, pmm_page_size());
    uint32_t pct = pmm_total() ? (pmm_total() - pmm_free()) * 100 / pmm_total()
                               : 0;
    printk("used: %u%%\n", pct);
}

static void cmd_heap(void) {
    printk("heap: %u KB reserved, %u KB in use\n",
           heap_total() / 1024, heap_used_bytes() / 1024);
}

static void cmd_irqstat(void) {
    printk("IRQ  count     (IRQ0-15)\n");
    for (uint8_t i = 0; i < 16; i++)
        if (irq_count(i))
            printk(" %2u  %u\n", i, irq_count(i));
    printk("total interrupts: %u\n", irq_total());
}

static void cmd_alloc(void) {
    void* p[8];
    for (int i = 0; i < 8; i++) {
        p[i] = kmalloc(1024 * (i + 1));
        if (!p[i]) {
            printk("alloc %d FAILED\n", i);
            return;
        }
    }
    printk("alloc 8 buffers ok\n");
    for (int i = 0; i < 8; i++)
        kfree(p[i]);
    printk("freed all ok\n");
}

static void cmd_sleep(const char* args) {
    uint32_t secs = (uint32_t)atoi(args);
    if (secs == 0 || secs > 60) {
        printk("usage: sleep 1-60\n");
        return;
    }
    uint32_t target = pit_ticks() + secs * pit_freq();
    while (pit_ticks() < target)
        hlt();
    printk("slept %u seconds\n", secs);
}

/* 解析十六进制参数 */
static int parse_hex(const char* s, uint32_t* out) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    uint32_t v = 0;
    while (*s) {
        char c = *s;
        uint32_t d;
        if (c >= '0' && c <= '9')      d = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
        else return 0;
        v = v * 16 + d;
        s++;
    }
    *out = v;
    return 1;
}

static void cmd_date(void) {
    rtc_print_time();
}

static void cmd_page(const char* args) {
    uint32_t vaddr;
    if (!parse_hex(args, &vaddr)) {
        printk("usage: page <hex address>\n");
        return;
    }
    uint32_t phys = paging_translate(vaddr);
    if (phys == 0xFFFFFFFFu)
        printk("va %p -> not present\n", (uintptr_t)vaddr);
    else
        printk("va %p -> pa %p\n", (uintptr_t)vaddr, (uintptr_t)phys);
}

static void cmd_map(const char* args) {
    char va_s[16];
    char fl_s[16];
    int n = 0;
    const char* p = args;
    while (*p && *p != ' ' && n < 15)
        va_s[n++] = *p++;
    va_s[n] = 0;
    while (*p == ' ')
        p++;
    n = 0;
    while (*p && *p != ' ' && n < 15)
        fl_s[n++] = *p++;
    fl_s[n] = 0;

    uint32_t vaddr, flags = PG_PRESENT | PG_RW;
    if (!parse_hex(va_s, &vaddr) || vaddr & 0xFFF) {
        printk("usage: map <hex va> [u=r]\n");
        return;
    }
    for (const char* f = fl_s; *f; f++) {
        if (*f == 'u')
            flags |= PG_USER;
        else if (*f != '=' && *f != 'r')
            flags = flags;   /* ignore */
    }

    /* 从 PMM 拿一页做映射目标 */
    uint32_t phys = pmm_alloc_page();
    if (!phys) {
        printk("no free physical page\n");
        return;
    }
    paging_map(vaddr, phys, flags);
    printk("map va %p -> pa %p flags=%x\n",
           (uintptr_t)vaddr, (uintptr_t)phys, flags);
}

static void cmd_unmap(const char* args) {
    uint32_t vaddr;
    if (!parse_hex(args, &vaddr)) {
        printk("usage: unmap <hex va>\n");
        return;
    }
    int r = paging_unmap(vaddr);
    printk(r ? "unmapped va %p\n" : "unmap failed (not mapped)\n",
           (uintptr_t)vaddr);
}

static void cmd_pgstat(void) {
    printk("paging: on, identity-mapped 0 - %u MB\n",
           paging_map_bytes() / (1024 * 1024));
    printk("page directory @ %p, size 4KB pages\n",
           (uintptr_t)paging_dir_frame());
    printk("page faults: %u, last addr=%p code=%x\n",
           paging_fault_count(),
           (uintptr_t)paging_last_fault_addr(),
           paging_last_fault_code());
}

static void cmd_fault(void) __attribute__((noreturn));
static void cmd_fault(void) {
    printk("triggering null-pointer write ...\n");
    *(volatile uint32_t*)0 = 0;
    for (;;)
        hlt();
}

static void cmd_about(void) {
    printk("WebOS: 32-bit multiboot kernel (i386)\n");
    printk("  pmm, heap, gdt, idt, pic, pit, rtc, keyboard, serial,\n");
    printk("  paging+vm, syscall(int 0x80), cpuinfo, pci\n");
    printk("  built " __TIME__ " " __DATE__ "\n");
}

static void cmd_serial(void) {
    printk("COM1: 115200 8N1, kernel console mirror active\n");
    printk("type into serial to feed the shell\n");
}

static void cmd_syscall(void) {
    uint32_t info[4];
    printk("syscall WRITE  -> %d chars\n",
           syscall_dispatch(SYSCALL_WRITE, 1,
                            (uint32_t)"hello from syscall!\n", 20));
    printk("syscall UPTIME -> %d ticks\n",
           syscall_dispatch(SYSCALL_UPTIME, 0, 0, 0));
    int ret = syscall_dispatch(SYSCALL_MEMINFO, 0, (uint32_t)info, 0);
    printk("syscall MEMINFO -> %d [phys %uK free %uK heap %uK used %uK]\n",
           ret, info[0] / 1024, info[1] / 1024,
           info[2] / 1024, info[3] / 1024);
}

static void cmd_test(void) {
    selftest_run();
}

static void cmd_history(void) {
    for (int i = 0; i < hist_count; i++)
        printk(" %d  %s\n", hist_count - i, hist[i]);
}

/* ---------- readline：单行编辑 + 历史 ---------- */

static void redraw_line(void) {
    terminal_erase_line();
    terminal_writestring(SHELL_PROMPT);
    terminal_write(input, input_len);
}

static void process_line(void);

void shell_run(void) {
    input_len = 0;
    terminal_writestring(SHELL_PROMPT);
    for (;;) {
        /* 串口输入优先（可用 -serial stdio 控制），再退到 PS/2 键盘 */
        int c = serial_getkey();
        if (c == KEY_NONE)
            c = keyboard_getkey_nonblock();
        if (c == KEY_NONE) {
            hlt();          /* 等中断唤醒，避免空转 */
            continue;
        }

        if (c == '\n' || c == '\r') {
            terminal_putchar('\n');
            input[input_len] = 0;
            hist_pos = -1;
            if (input[0]) {
                /* 入历史（去重） */
                if (hist_count == 0 || strcmp(hist[0], input) != 0) {
                    for (int i = HIST_MAX - 1; i > 0; i--)
                        strncpy(hist[i], hist[i - 1], INPUT_MAX - 1);
                    strncpy(hist[0], input, INPUT_MAX - 1);
                    if (hist_count < HIST_MAX)
                        hist_count++;
                }
            }
            process_line();
            input_len = 0;
            terminal_writestring(SHELL_PROMPT);
        } else if (c == '\b' || (c < 0x100 && c == 0x7F)) {
            if (input_len > 0) {
                input_len--;
                terminal_putchar('\b');
            }
        } else if (c == KEY_UP) {
            if (hist_count > 0) {
                if (input_len == 0 && hist_pos < 0)
                    hist_pos = 0;
                else if (hist_pos >= 0 && hist_pos < hist_count - 1)
                    hist_pos++;
                else if (hist_pos < 0)
                    hist_pos = 0;
                if (hist_pos >= 0) {
                    strncpy(input, hist[hist_pos], INPUT_MAX - 1);
                    input[INPUT_MAX - 1] = 0;
                    input_len = strlen(input);
                    redraw_line();
                }
            }
        } else if (c == KEY_DOWN) {
            if (hist_pos > 0) {
                hist_pos--;
                strncpy(input, hist[hist_pos], INPUT_MAX - 1);
                input[INPUT_MAX - 1] = 0;
                input_len = strlen(input);
                redraw_line();
            } else if (hist_pos == 0) {
                hist_pos = -1;
                input_len = 0;
                input[0] = 0;
                redraw_line();
            }
        } else if (c == KEY_ESC) {
            /* 忽略 */
        } else if (c >= 0x20 && c < 0x7F && input_len < INPUT_MAX - 1) {
            input[input_len++] = (char)c;
            terminal_putchar((char)c);
        }
    }
}

void hist_load(void);
static void process_line(void)
{
    /* 跳过前导空格 */
    const char* line = input;
    while (*line == ' ')
        line++;

    /* 取出第一个词 */
    char cmd[32];
    size_t i = 0;
    while (line[i] && line[i] != ' ' && i < sizeof(cmd) - 1) {
        cmd[i] = line[i];
        i++;
    }
    cmd[i] = 0;

    /* 参数（跳过空格） */
    const char* args = line + i;
    while (*args == ' ')
        args++;

    if (!cmd[0]) {
        /* 空行 */
    } else if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        terminal_clear();
    } else if (strcmp(cmd, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(cmd, "uptime") == 0) {
        printk("up %u seconds (%u ticks @ %u Hz)\n",
               pit_ticks() / pit_freq(), pit_ticks(), pit_freq());
    } else if (strcmp(cmd, "mem") == 0) {
        cmd_mem();
    } else if (strcmp(cmd, "memmap") == 0) {
        cmd_memmap();
    } else if (strcmp(cmd, "heap") == 0) {
        cmd_heap();
    } else if (strcmp(cmd, "irqstat") == 0) {
        cmd_irqstat();
    } else if (strcmp(cmd, "alloc") == 0) {
        cmd_alloc();
    } else if (strcmp(cmd, "sleep") == 0) {
        cmd_sleep(args);
    } else if (strcmp(cmd, "date") == 0) {
        cmd_date();
    } else if (strcmp(cmd, "page") == 0) {
        cmd_page(args);
    } else if (strcmp(cmd, "map") == 0) {
        cmd_map(args);
    } else if (strcmp(cmd, "unmap") == 0) {
        cmd_unmap(args);
    } else if (strcmp(cmd, "pgstat") == 0) {
        cmd_pgstat();
    } else if (strcmp(cmd, "cpuinfo") == 0 ||
               strcmp(cmd, "cpuid") == 0) {
        cpuinfo_print();
    } else if (strcmp(cmd, "pci") == 0) {
        pci_scan();
    } else if (strcmp(cmd, "serial") == 0) {
        cmd_serial();
    } else if (strcmp(cmd, "syscall") == 0) {
        cmd_syscall();
    } else if (strcmp(cmd, "test") == 0) {
        cmd_test();
    } else if (strcmp(cmd, "history") == 0) {
        cmd_history();
    } else if (strcmp(cmd, "fault") == 0) {
        cmd_fault();
    } else if (strcmp(cmd, "about") == 0) {
        cmd_about();
    } else if (strcmp(cmd, "reboot") == 0) {
        reboot();
    } else if (strcmp(cmd, "shutdown") == 0 ||
               strcmp(cmd, "poweroff") == 0) {
        shutdown();
    } else if (strcmp(cmd, "halt") == 0) {
        printk("Halting CPU.\n");
        for (;;)
            hlt();
    } else {
        printk("unknown command: %s   (try 'help')\n", cmd);
    }
}
