#!/bin/bash
# WebOS 构建脚本：生成本地 ELF（模拟环境）或 i686 引导内核 + 可选 ISO
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build"
ISO="$ROOT/iso"

CROSS="i686-elf"
if command -v "${CROSS}-gcc" >/dev/null 2>&1; then
    CC="${CROSS}-gcc"
    AS="${CROSS}-as"
    LD="${CROSS}-ld"
else
    echo "[WebOS] 未找到 ${CROSS}-gcc，尝试本机 clang带本机 clang 编译（不推荐运行）"
    CC=clang
    AS=false
    LD=ld
fi

CFLAGS="-m32 -ffreestanding -fno-builtin -fno-stack-protector -fno-exceptions \
-fno-asynchronous-unwind-tables -nostdlib -I$ROOT/kernel -Wall -Wextra -O2"

SRCS_C="kernel/libc.c kernel/pmm.c kernel/heap.c kernel/terminal.c kernel/printk.c kernel/gdt.c \
kernel/idt.c kernel/pic.c kernel/pit.c kernel/keyboard.c \
kernel/paging.c kernel/rtc.c \
kernel/serial.c kernel/syscall.c kernel/cpuinfo.c kernel/pci.c kernel/selftest.c \
kernel/shell.c kernel/reboot.c kernel/main.c"

mkdir -p "$BUILD" "$ISO/boot/grub"

echo "[WebOS] 编译器: $CC"

# 1) 汇编
echo "[WebOS] 汇编 boot/boot.S ..."
$AS --32 boot/boot.S -o "$BUILD/boot.o"
$AS --32 boot/gdt_flush.S -o "$BUILD/gdt_flush.o"
$AS --32 kernel/isr.S -o "$BUILD/isr.o"

# 2) C
OBJS=()
for src in $SRCS_C; do
    obj="$BUILD/$(basename "$src" .c).o"
    echo "[WebOS] 编译 $src -> $(basename "$obj")"
    $CC $CFLAGS -c "$ROOT/$src" -o "$obj"
    OBJS+=("$obj")
done

# 3) 链接
echo "[WebOS] 链接 build/WebOS.elf ..."
$LD -T "$ROOT/kernel/linker.ld" -o "$BUILD/WebOS.elf" \
    "$BUILD/boot.o" "$BUILD/gdt_flush.o" "$BUILD/isr.o" "${OBJS[@]}"
echo "[WebOS] 生成: $BUILD/WebOS.elf"

# 4) 运行模拟（无 qemu 时报告）
if command -v qemu-system-i386 >/dev/null 2>&1; then
    echo "[WebOS] 运行: qemu-system-i386 -kernel $BUILD/WebOS.elf"
    if [ "${1:-}" = "--run" ]; then
        exec qemu-system-i386 -drive file=/dev/null -kernel "$BUILD/WebOS.elf" -no-reboot
    fi
else
    echo "[WebOS] qemu-system-i386 未安装（brew install qemu 可运行）"
fi

# 5) ISO（默认跳过，--iso 开启； grub-mkrescue 优先用 i686-elf 版本）
GRUB_MKRESCUE="grub-mkrescue"
command -v i686-elf-grub-mkrescue >/dev/null 2>&1 && GRUB_MKRESCUE="i686-elf-grub-mkrescue"

if [ "${1:-}" = "--iso" ] || [ "${2:-}" = "--iso" ]; then
    if command -v "$GRUB_MKRESCUE" >/dev/null 2>&1; then
        if [ ! -f "$ISO/boot/grub/grub.cfg" ]; then
            cat > "$ISO/boot/grub/grub.cfg" <<EOF
set timeout=0
set default=0

menuentry "WebOS" {
    multiboot /boot/WebOS.elf
    boot
}
EOF
        fi
        cp "$BUILD/WebOS.elf" "$ISO/boot/WebOS.elf"
        echo "[WebOS] 生成 ISO: build/WebOS.iso"
        "$GRUB_MKRESCUE" -o "$BUILD/WebOS.iso" "$ISO"
    else
        echo "[WebOS] grub-mkrescue 未安装"
    fi
fi

echo "[WebOS] 构建完成"
