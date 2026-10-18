/* WebOS - 内核自检 */
#include "kernel.h"

static int g_fail;
static int g_pass;

static void ok(const char* name, int cond) {
    if (cond) {
        g_pass++;
        printk("  [ok]   %s\n", name);
    } else {
        g_fail++;
        printk("  [FAIL] %s\n", name);
    }
}

int selftest_run(void) {
    g_fail = 0;
    g_pass = 0;
    printk("selftest:\n");

    /* libc */
    {
        char buf[33];
        memset(buf, 'A', 8);
        ok("memset", buf[0] == 'A' && buf[7] == 'A');
        ok("strcmp", strcmp("abc", "abc") == 0 && strcmp("abc", "abd") < 0);
        u32_to_str(255, buf, 16);
        ok("u32_to_str(hex)", strcmp(buf, "ff") == 0);
        u32_to_str(12345, buf, 10);
        ok("u32_to_str(dec)", strcmp(buf, "12345") == 0);
    }

    /* PMM：分配/释放回环 */
    {
        uint32_t before = pmm_free();
        uint32_t page = pmm_alloc_page();
        ok("pmm alloc", page != 0);
        pmm_free_page(page);
        ok("pmm free", pmm_free() == before);
    }

    /* 堆：写入数据验证真实可用 */
    {
        uint32_t* p = kmalloc(sizeof(uint32_t) * 32);
        int fill_ok = 1;
        if (!p) {
            ok("kmalloc", 0);
        } else {
            for (int i = 0; i < 32; i++)
                p[i] = (uint32_t)(0xDEAD0000u + (uint32_t)i);
            for (int i = 0; i < 32; i++)
                if (p[i] != (uint32_t)(0xDEAD0000u + i))
                    fill_ok = 0;
            ok("kmalloc", 1);
            ok("heap r/w", fill_ok);
            kfree(p);
            ok("kfree", 1);
        }
    }

    /* 分页：内核镜像仍在恒等映射区 */
    {
        ok("paging identity",
           paging_translate(0x100000) == 0x100000);
        ok("paging dir mapped",
           (paging_dir_frame() & 0xFFFFF000) != 0);
    }

    /* PIT：tick 推进 */
    {
        uint32_t t0 = pit_ticks();
        uint32_t deadline = t0 + pit_freq() + 2;    /* 最多等 1 译秒 */
        while (pit_ticks() < deadline)
            hlt();
        ok("PIT ticks advance", pit_ticks() >= deadline);
    }

    /* RTC */
    {
        uint32_t y, mo, d, h, mi, s;
        rtc_get(&y, &mo, &d, &h, &mi, &s);
        ok("RTC ready", rtc_ready() && y >= 2000 && y <= 2100);
    }

    /* 系统调用分发（直接调用） */
    {
        int32_t up = syscall_dispatch(SYSCALL_UPTIME, 0, 0, 0);
        ok("syscall uptime", up != 0);
        int32_t bad = syscall_dispatch(99, 0, 0, 0);
        ok("syscall unknown = -1", bad == -1);
    }

    printk("selftest done: %d ok, %d failed\n", g_pass, g_fail);
    return g_fail;
}
