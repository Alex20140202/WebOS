/* WebOS - printk：可变参数格式化输出
   支持 %s %c %d %i %u %x %X %p %%，以及 - 左对齐、0 补零、数字宽度。 */
#include <stdarg.h>
#include "kernel.h"

static void emit_padded(const char* str, int width, char pad) {
    int len = (int)strlen(str);
    for (int i = len; i < width; i++)
        terminal_putchar(pad);
    terminal_writestring(str);
}

static void emit_number(uint32_t value, int base, int uppercase,
                        int width, char pad) {
    char buf[33];
    u32_to_str(value, buf, base);
    if (uppercase) {
        for (char* p = buf; *p; p++)
            if (*p >= 'a' && *p <= 'f')
                *p = (char)(*p - 'a' + 'A');
    }
    emit_padded(buf, width, pad);
}

int printk(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int count = 0;

    for (const char* p = fmt; *p; p++) {
        if (*p != '%') {
            terminal_putchar(*p);
            count++;
            continue;
        }

        p++;
        int left_align = 0;
        char pad = ' ';
        int width = 0;

        while (*p == '-' || *p == '0' || *p == '+' || *p == ' ') {
            if (*p == '-')
                left_align = 1;
            else if (*p == '0')
                pad = '0';
            p++;
        }
        while (is_digit(*p))
            width = width * 10 + (*p++ - '0');

        switch (*p) {
        case 's': {
            const char* s = va_arg(ap, const char*);
            const char* str = s ? s : "(null)";
            if (left_align)
                terminal_writestring(str);
            else
                emit_padded(str, width, ' ');
            count += (int)strlen(str);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (!left_align)
                emit_padded("", width - 1, ' ');   /* cwidth=1 */
            terminal_putchar(c);
            count++;
            break;
        }
        case 'd':
        case 'i': {
            int32_t v = va_arg(ap, int32_t);
            uint32_t uv;
            char sign = 0;
            if (v < 0) {
                sign = '-';
                uv = (uint32_t)(-v);
            } else {
                uv = (uint32_t)v;
            }
            char head[2] = { sign, 0 };
            if (sign || pad == '0')
                width -= (int)strlen(head);
            if (left_align) {
                terminal_writestring(head);
                emit_number(uv, 10, 0, width, ' ');
            } else {
                if (pad == '0')
                    emit_padded(head, width < 0 ? 0 : width, '0');
                emit_number(uv, 10, 0, width < 0 ? 0 : width, '0');
            }
            break;
        }
        case 'u': {
            uint32_t uv = va_arg(ap, uint32_t);
            if (left_align) {
                emit_number(uv, 10, 0, width, ' ');
            } else {
                emit_number(uv, 10, 0, width, pad == '0' ? '0' : ' ');
            }
            break;
        }
        case 'x':
        case 'X': {
            uint32_t uv = va_arg(ap, uint32_t);
            emit_number(uv, 16, *p == 'X', width,
                        left_align ? ' ' : (pad == '0' ? '0' : ' '));
            break;
        }
        case 'p': {
            uintptr_t v = va_arg(ap, uintptr_t);
            emit_padded("0x", 0, ' ');
            count += 2;
            emit_number((uint32_t)v, 16, 0, width, pad == '0' ? '0' : ' ');
            break;
        }
        case '%':
            terminal_putchar('%');
            count++;
            break;
        default:
            terminal_putchar('%');
            terminal_putchar(*p ? *p : '?');
            count += 2;
            break;
        }
    }
    va_end(ap);
    return count;
}