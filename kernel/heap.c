/* WebOS - 内核堆：kmalloc/kfree，first-fit 空闲链表 + 4KB 页池（PMM 提供） */
#include "kernel.h"

struct heap_block {
    uint32_t size;          /* 含头部 */
    uint32_t used;          /* 0 = 空闲 */
    struct heap_block* next;
    struct heap_block* prev;
};                              /* 4 * 4B = 16B，本身即 8 字节对齐 */

#define HEAP_ALIGN 8
#define HEAP_HEADER_SIZE 16      /* 必须等于 sizeof(struct heap_block) */
#define HEAP_PAGE (pmm_page_size())

static struct heap_block* heap_head;
static uint32_t heap_size;
static uint32_t heap_used;
static int heap_ready;

void heap_init(void) {
    heap_head = NULL;
    heap_size = 0;
    heap_used = 0;
    heap_ready = 1;
}

static void heap_add_page(void) {
    uint32_t phys = pmm_alloc_page();
    if (!phys)
        return;
    struct heap_block* b = (struct heap_block*)phys;
    b->size = HEAP_PAGE;
    b->used = 0;
    b->next = NULL;
    b->prev = NULL;

    if (!heap_head) {
        heap_head = b;
    } else {
        struct heap_block* last = heap_head;
        while (last->next)
            last = last->next;
        /* 与最后一块物理相邻则扩容，否则新增独立块 */
        if ((uint32_t)last + last->size == phys) {
            last->size += HEAP_PAGE;
        } else {
            last->next = b;
            b->prev = last;
        }
    }
    heap_size += HEAP_PAGE;
}

static void* block_alloc(struct heap_block* b, uint32_t need) {
    uint32_t remain = b->size - need;
    if (remain >= HEAP_HEADER_SIZE + HEAP_ALIGN) {
        /* 切分 */
        struct heap_block* split = (struct heap_block*)((char*)b + need);
        split->size = remain;
        split->used = 0;
        split->prev = b;
        split->next = b->next;
        if (split->next)
            split->next->prev = split;
        b->size = need;
        b->next = split;
    }
    b->used = 1;
    heap_used += b->size;
    return (char*)b + HEAP_HEADER_SIZE;
}

void* kmalloc(size_t size) {
    if (!heap_ready || size == 0)
        return NULL;

    uint32_t need = (size + HEAP_HEADER_SIZE + HEAP_ALIGN - 1)
                    & ~(HEAP_ALIGN - 1);

    /* first-fit 搜索空闲块 */
    struct heap_block* b = heap_head;
    while (b) {
        if (!b->used && b->size >= need)
            return block_alloc(b, need);
        b = b->next;
    }

    /* 不够：扩页（先充几个 4KB 页，最多重试 4 次找连续块） */
    for (int tries = 0; tries < 4; tries++) {
        heap_add_page();
        b = heap_head;
        while (b) {
            if (!b->used && b->size >= need)
                return block_alloc(b, need);
            b = b->next;
        }
    }
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr)
        return;
    struct heap_block* b = (struct heap_block*)((char*)ptr - HEAP_HEADER_SIZE);
    if (!b->used)
        return;             /* 双重释放忽略 */
    b->used = 0;
    heap_used -= b->size;

    /* 向前合并（b->prev 指向列表中的前一块） */
    struct heap_block* p = b->prev;
    if (p && !p->used && (char*)p + p->size == (char*)b) {
        p->size += b->size;
        p->next = b->next;
        if (p->next)
            p->next->prev = p;
        b = p;
    }
    /* 向后合并 */
    struct heap_block* n = b->next;
    if (n && !n->used && (char*)b + b->size == (char*)n) {
        b->size += n->size;
        b->next = n->next;
        if (n->next)
            n->next->prev = b;
    }
}

void* kzalloc(size_t size) {
    void* p = kmalloc(size);
    if (p)
        memset(p, 0, size);
    return p;
}

uint32_t heap_total(void)  { return heap_size; }
uint32_t heap_used_bytes(void) { return heap_used; }
int heap_is_ready(void) { return heap_ready; }
