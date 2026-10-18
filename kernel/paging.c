/* WebOS - 分页（虚拟内存）：页目录 + 恒等映射低 16MB，页错误统计 */
#include "kernel.h"

#define PG_PTE_PRESENT 0x001
#define PG_PTE_RW      0x002
#define PG_PTE_ADDR    0xFFFFF000u
#define PG_PTE_USER    0x004

#define LOW_MAP_BYTES (16 * 1024 * 1024)   /* 恒等映射范围 */

/* bss 静态缓冲：页目录 + 4 张页表（覆盖 16MB，均 4KB 对齐） */
static uint32_t page_dir[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[4][1024] __attribute__((aligned(4096)));

static uint32_t fault_count;
static uint32_t last_fault_addr;
static uint32_t last_fault_code;

void paging_enable(const uint32_t* pd) {
    __asm__ volatile(
        "movl %0, %%cr3\n"
        "movl %%cr0, %%eax\n"
        "orl $0x80000000, %%eax\n"      /* PG 位 */
        "movl %%eax, %%cr0\n"
        : : "r"(pd) : "eax", "memory"
    );
}

void paging_init(void) {
    /* 页目录前 4 项指向页表（P + RW），其余清零 */
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 1024; j++)
            page_tables[i][j] =
                ((i * 1024 + j) * 4096) | PG_PTE_PRESENT | PG_PTE_RW;
        page_dir[i] = (uint32_t)page_tables[i] | PG_PTE_PRESENT | PG_PTE_RW;
    }
    for (int i = 4; i < 1024; i++)
        page_dir[i] = 0;

    fault_count = 0;
    paging_enable(page_dir);
}

uint32_t paging_fault_count(void)        { return fault_count; }
uint32_t paging_last_fault_addr(void)    { return last_fault_addr; }
uint32_t paging_last_fault_code(void)    { return last_fault_code; }

void paging_page_fault(uint32_t virt, uint32_t code) {
    fault_count++;
    last_fault_addr = virt;
    last_fault_code = code;
}

/* 简单地址翻译：返回物理地址，未映射返回 0xFFFFFFFF */
uint32_t paging_translate(uint32_t virt) {
    uint32_t pdi = (virt >> 22) & 0x3FF;
    uint32_t pti = (virt >> 12) & 0x3FF;
    uint32_t pde = page_dir[pdi];
    if (!(pde & PG_PTE_PRESENT))
        return 0xFFFFFFFFu;
    uint32_t pte = ((uint32_t*)(pde & PG_PTE_ADDR))[pti];
    if (!(pte & PG_PTE_PRESENT))
        return 0xFFFFFFFFu;
    return (pte & PG_PTE_ADDR) | (virt & 0xFFF);
}

uint32_t paging_dir_frame(void) {
    uint32_t cr3;
    __asm__ volatile("movl %%cr3, %0" : "=r"(cr3));
    return cr3;
}

uint32_t paging_map_bytes(void) { return LOW_MAP_BYTES; }

/* 取（或按需分配）pdi 对应的页表，返回 0 表示无法建立 */
static uint32_t* paging_table_for(uint32_t pdi, int create) {
    if (pdi < 4)
        return page_tables[pdi];

    uint32_t pde = page_dir[pdi];
    if (pde & PG_PTE_PRESENT)
        return (uint32_t*)(pde & PG_PTE_ADDR);

    if (!create)
        return NULL;

    uint32_t frame = pmm_alloc_page();
    if (!frame)
        return NULL;
    memset((void*)frame, 0, 4096);
    page_dir[pdi] = frame | PG_PTE_PRESENT | PG_PTE_RW;
    return (uint32_t*)frame;
}

static inline void paging_flush(uint32_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* 建立映射；vaddr/paddr 需 4KB 对齐。成功 0，失败 -1 */
int paging_map(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    if (vaddr & 0xFFF || paddr & 0xFFF)
        return -1;

    uint32_t pdi = (vaddr >> 22) & 0x3FF;
    uint32_t pti = (vaddr >> 12) & 0x3FF;
    uint32_t* pt = paging_table_for(pdi, 1);
    if (!pt)
        return -1;

    uint32_t f = PG_PTE_PRESENT;
    if (flags & PG_RW)      f |= PG_PTE_RW;
    if (flags & PG_USER)    f |= PG_PTE_USER;
    if (flags & PG_PRESENT) f |= PG_PTE_PRESENT;
    pt[pti] = (paddr & PG_PTE_ADDR) | f;
    paging_flush(vaddr);
    return 0;
}

/* 解除映射；成功 0，未映射 -1 */
int paging_unmap(uint32_t vaddr) {
    if (vaddr & 0xFFF)
        return -1;

    uint32_t pdi = (vaddr >> 22) & 0x3FF;
    uint32_t pti = (vaddr >> 12) & 0x3FF;
    uint32_t pde = page_dir[pdi];
    if (!(pde & PG_PTE_PRESENT))
        return -1;

    uint32_t* pt = (uint32_t*)(pde & PG_PTE_ADDR);
    if (!(pt[pti] & PG_PTE_PRESENT))
        return -1;

    pt[pti] = 0;
    paging_flush(vaddr);
    return 0;
}
