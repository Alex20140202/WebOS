/* WebOS - 物理内存管理器：解析 multiboot 内存映射，bitmap 分配/释放 4KB 页帧 */
#include "kernel.h"

#define PMM_PAGE_SIZE 4096

/* bitmap：每 bit 对应一页，1 = 已占用 */
static uint32_t* pmm_bitmap;
static uint32_t  pmm_bitmap_pages;   /* bitmap 自身覆盖的页数 */
static uint32_t  pmm_total_pages;
static uint32_t  pmm_used_pages;
static uint32_t  pmm_max_addr;       /* 已知最高可用内存地址 */

struct multiboot_mmap_entry {
    uint32_t size;          /* 本条目大小（含 type），multiboot 规格 */
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

#define MMAP_TYPE_AVAILABLE 1

static inline int bitmap_test(uint32_t page) {
    return pmm_bitmap[page >> 5] & (1u << (page & 31));
}

static inline void bitmap_set(uint32_t page) {
    pmm_bitmap[page >> 5] |= (1u << (page & 31));
}

static inline void bitmap_clear(uint32_t page) {
    pmm_bitmap[page >> 5] &= ~(1u << (page & 31));
}

void pmm_init(uint32_t mbinfo) {
    /* 失败路径：无 mmap 时退化为保底 64MB 可用区 */
    pmm_total_pages = 0;
    pmm_bitmap = NULL;
    pmm_bitmap_pages = 0;
    pmm_max_addr = 0;

    /* flags 字段在 mbinfo 偏移 0，bit6 = mmap 存在 */
    if (mbinfo && (*(uint32_t*)mbinfo & (1u << 6))) {
        uint32_t mmap_len   = *(uint32_t*)(mbinfo + 44 + 0);
        uint32_t mmap_addr  = *(uint32_t*)(mbinfo + 44 + 4);

        /* 第一遍：统计最高可用地址 */
        uint32_t off = mmap_addr;
        while (off < mmap_addr + mmap_len) {
            struct multiboot_mmap_entry* e = (struct multiboot_mmap_entry*)off;
            if (e->type == MMAP_TYPE_AVAILABLE &&
                (uint32_t)(e->addr + e->len) > pmm_max_addr)
                pmm_max_addr = (uint32_t)(e->addr + e->len);
            off += e->size + 4;
        }
        if (pmm_max_addr)
            pmm_total_pages = pmm_max_addr / PMM_PAGE_SIZE;
    }
    if (!pmm_total_pages) {
        pmm_max_addr = 64 * 1024 * 1024;
        pmm_total_pages = pmm_max_addr / PMM_PAGE_SIZE;
    }

    /* bitmap 放在内核镜像末尾之后（链接脚本符号），页对齐 */
    pmm_bitmap_pages = (pmm_total_pages + 31) / 32;
    uint32_t bm_bytes = (pmm_bitmap_pages * 4 + PMM_PAGE_SIZE - 1)
                        & ~(PMM_PAGE_SIZE - 1);
    extern char _kernel_end[];
    uint32_t bm_addr = ((uint32_t)_kernel_end + PMM_PAGE_SIZE - 1)
                       & ~(PMM_PAGE_SIZE - 1);
    pmm_bitmap = (uint32_t*)bm_addr;
    memset(pmm_bitmap, 0, bm_bytes);

    /* 先全部标记占用，再把可用区逐一释放 */
    pmm_used_pages = pmm_total_pages;
    memset(pmm_bitmap, 0xFF, bm_bytes);

    if (mbinfo && (*(uint32_t*)mbinfo & (1u << 6))) {
        uint32_t mmap_len   = *(uint32_t*)(mbinfo + 44);
        uint32_t mmap_addr  = *(uint32_t*)(mbinfo + 48);
        uint32_t off = mmap_addr;
        while (off < mmap_addr + mmap_len) {
            struct multiboot_mmap_entry* e = (struct multiboot_mmap_entry*)off;
            if (e->type == MMAP_TYPE_AVAILABLE) {
                uint32_t start = (uint32_t)(e->addr + PMM_PAGE_SIZE - 1)
                                 / PMM_PAGE_SIZE;
                uint32_t end   = (uint32_t)(e->addr + e->len) / PMM_PAGE_SIZE;
                for (uint32_t p = start; p < end && p < pmm_total_pages; p++) {
                    if (bitmap_test(p)) {
                        bitmap_clear(p);
                        pmm_used_pages--;
                    }
                }
            }
            off += e->size + 4;
        }
    }

    extern char _kernel_start[];
    uint32_t kstart = (uint32_t)_kernel_start / PMM_PAGE_SIZE;
    uint32_t kend = ((uint32_t)_kernel_end + bm_bytes + PMM_PAGE_SIZE - 1)
                    / PMM_PAGE_SIZE;
    for (uint32_t p = 0; p < kstart; p++)
        if (!bitmap_test(p)) { bitmap_set(p); pmm_used_pages++; }
    for (uint32_t p = kstart; p < kend && p < pmm_total_pages; p++)
        if (!bitmap_test(p)) { bitmap_set(p); pmm_used_pages++; }
}

uint32_t pmm_alloc_page(void) {
    if (!pmm_bitmap)
        return 0;
    for (uint32_t p = 0; p < pmm_total_pages; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            pmm_used_pages++;
            return p * PMM_PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_page(uint32_t phys) {
    uint32_t p = phys / PMM_PAGE_SIZE;
    if (p >= pmm_total_pages || !bitmap_test(p))
        return;                     /* 重复释放/越界直接忽略 */
    bitmap_clear(p);
    pmm_used_pages--;
}

void pmm_alloc_range(uint32_t phys, uint32_t len) {
    uint32_t start = phys / PMM_PAGE_SIZE;
    uint32_t end = (phys + len + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    for (uint32_t p = start; p < end && p < pmm_total_pages; p++)
        if (!bitmap_test(p)) { bitmap_set(p); pmm_used_pages++; }
}

uint32_t pmm_total(void)  { return pmm_total_pages * PMM_PAGE_SIZE; }
uint32_t pmm_free(void)   { return (pmm_total_pages - pmm_used_pages) * PMM_PAGE_SIZE; }
uint32_t pmm_page_size(void) { return PMM_PAGE_SIZE; }

uint32_t pmm_alloc_pages(uint32_t count) {
    if (!pmm_bitmap || count == 0)
        return 0;
    uint32_t run = 0;
    for (uint32_t p = 0; p < pmm_total_pages; p++) {
        if (!bitmap_test(p)) {
            run++;
            if (run == count) {
                uint32_t first = p - count + 1;
                for (uint32_t q = first; q <= p; q++) {
                    bitmap_set(q);
                    pmm_used_pages++;
                }
                return first * PMM_PAGE_SIZE;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}
