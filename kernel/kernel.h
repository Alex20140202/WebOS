#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef int                 int32_t;
typedef long long           int64_t;
typedef unsigned long long  uint64_t;
typedef unsigned long       uintptr_t;
typedef unsigned long       size_t;
typedef int32_t             ssize_t;
typedef uint32_t            ssize32_t;

#define NULL ((void*)0)
#define UNUSED(x) (void)(x)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

/* VGA 文本模式缓冲区地址 */
#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* VGA 颜色 */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return (uint8_t)fg | (uint8_t)bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | (uint16_t)color << 8;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t cr2_read(void) {
    uint32_t v;
    __asm__ volatile("movl %%cr2, %0" : "=r"(v));
    return v;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/* libc 子集 */
size_t strlen(const char* str);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, size_t n);
char* strchr(const char* s, int c);
void* memset(void* dst, int val, size_t n);
void* memcpy(void* dst, const void* src, size_t n);

/* 数字格式化（自己实现，不依赖 libc） */
void u32_to_str(uint32_t value, char* buf, int base);   /* buf 至少 33 字节 */
char* strcat(char* dst, const char* src);
int atoi(const char* s);                                /* 十进制，失败 0 */

/* 物理内存管理器 */
void pmm_init(uint32_t mbinfo);
uint32_t pmm_alloc_page(void);
void pmm_free_page(uint32_t phys);
void pmm_alloc_range(uint32_t phys, uint32_t len);
uint32_t pmm_total(void);
uint32_t pmm_free(void);
uint32_t pmm_page_size(void);
uint32_t pmm_alloc_pages(uint32_t count);   /* 连续页；失败返回 0 */

/* 内核堆 */
void heap_init(void);
void* kmalloc(size_t size);
void* kzalloc(size_t size);
void kfree(void* ptr);
uint32_t heap_total(void);
uint32_t heap_used_bytes(void);
int heap_is_ready(void);

/* 重启 / 关机 */
void reboot(void);
void shutdown(void);

/* 分页（虚拟内存） */
#define PG_PRESENT 0x001
#define PG_RW      0x002
#define PG_USER    0x004
void paging_init(void);
int  paging_map(uint32_t vaddr, uint32_t paddr, uint32_t flags);   /* 成功 0 */
int paging_unmap(uint32_t vaddr);
void paging_enable(const uint32_t* pd);
void paging_page_fault(uint32_t virt, uint32_t code);
uint32_t paging_fault_count(void);
uint32_t paging_last_fault_addr(void);
uint32_t paging_last_fault_code(void);
uint32_t paging_translate(uint32_t virt);
uint32_t paging_dir_frame(void);
uint32_t paging_map_bytes(void);

/* CMOS RTC */
void rtc_init(void);
int rtc_ready(void);
void rtc_print_time(void);
void rtc_get(uint32_t* year, uint32_t* month, uint32_t* day,
             uint32_t* hour, uint32_t* minute, uint32_t* second);

/* ctype 子集 */
int is_digit(int c);
int is_hex(int c);
int is_space(int c);

/* VGA 终端 */
void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_clear(void);
void terminal_set_cursor(size_t row, size_t col);
void terminal_get_cursor(size_t* row, size_t* col);
void terminal_erase_line(void);             /* 清除光标所在整行并回到列 0 */
void terminal_erase_to_end(void);           /* 清除光标到行尾 */
void terminal_scroll(void);

/* printf 风格精简版：%s %c %d %u %x %8x %p %%，返回打印字符数 */
int printk(const char* fmt, ...);

/* GDT */
void gdt_init(void);

/* IDT / 中断 */
void idt_init(void);
const char* exception_name(uint32_t n);

/* PIC */
void pic_remap(uint8_t offset1, uint8_t offset2);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint16_t mask);   /* bit=1 屏蔽对应 IRQ */
uint16_t pic_get_mask(void);

/* PIT 时钟 */
void pit_init(uint32_t freq);
uint32_t pit_ticks(void);
uint32_t pit_freq(void);
void pit_sleep_ms(uint32_t ms);     /* hlt 等待，可被中断唤醒 */

/* IRQ 统计（idt.c 维护） */
uint32_t irq_count(uint8_t irq);        /* IRQ0-15 累计触发次数 */
uint32_t irq_total(void);

/* PS/2 键盘 */
enum special_key {
    KEY_NONE = 0,
    KEY_UP = 0x100, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_PAGEUP, KEY_PAGEDOWN,
    KEY_DELETE, KEY_INSERT, KEY_ESC,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
};
void keyboard_init(void);
int keyboard_has_key(void);
int keyboard_getkey(void);          /* ASCII 或 special_key，阻塞 */
int keyboard_getkey_nonblock(void); /* 无键返回 KEY_NONE */

/* 串口 COM1：内核控制台镜像 + shell 输入源 */
void serial_init(void);
void serial_putchar(char c);
int serial_has_key(void);
int serial_getkey(void);            /* 无键返回 KEY_NONE */
void serial_writestring(const char* s);

/* CPUID */
void cpuinfo_print(void);           /* 打印 vendor/brand/flags */
uint32_t cpuinfo_features(void);    /* edx 特性位（leaf 1） */
void cpuinfo_vendor(char out[13]);  /* 12 字符 + NUL */

/* PCI 配置空间 */
uint32_t pci_config_read(uint32_t bus, uint32_t slot, uint32_t func,
                         uint8_t offset);
void pci_scan(void);                /* 枚举并打印设备 */

/* 系统调用（int 0x80） */
#define SYSCALL_WRITE   0   /* ebx=fd(1=console) ecx=buf edx=len -> 返回写入数 */
#define SYSCALL_EXIT    1   /* ebx=code（当前实现为挂起） */
#define SYSCALL_UPTIME  2   /* 返回 ticks */
#define SYSCALL_SLEEP   3   /* ebx=ms */
#define SYSCALL_MEMINFO 4   /* ecx=写入 uint32_t[4]: total,free,heap_total,heap_used */
#define SYSCALL_MAX     4
void syscall_init(void);            /* 安装 int 0x80 门（由 idt_init 调用） */
int32_t syscall_dispatch(uint32_t num, uint32_t a, uint32_t b, uint32_t c);
extern void syscall_stub(void);     /* isr.S 提供 */

/* 内核自检 */
int selftest_run(void);             /* 返回失败项数 */

/* 内核入口 */
void kernel_main(uint32_t magic, uint32_t mbinfo);

/* shell */
void shell_run(void);

/* 链接脚本提供（cmd_mem 使用） */
extern char _kernel_start[];
extern char _kernel_end[];

#endif
