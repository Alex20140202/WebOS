/* WebOS - 迷你 libc：内核自用，无外部依赖 */
#include "kernel.h"

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i])
            return 0;
    }
    return 0;
}

char* strcpy(char* dst, const char* src) {
    char* p = dst;
    while ((*p++ = *src++))
        ;
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
    return dst;
}

char* strchr(const char* s, int c) {
    for (; *s; s++) {
        if (*s == (char)c)
            return (char*)s;
    }
    return (c == 0) ? (char*)s : NULL;
}

void* memset(void* dst, int val, size_t n) {
    unsigned char* p = dst;
    while (n--)
        *p++ = (unsigned char)val;
    return dst;
}

void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = dst;
    const unsigned char* s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

char* strcat(char* dst, const char* src) {
    char* p = dst + strlen(dst);
    while ((*p++ = *src++))
        ;
    return dst;
}

void u32_to_str(uint32_t value, char* buf, int base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[33];
    int i = 0;
    if (base < 2)
        base = 10;
    if (value == 0)
        tmp[i++] = '0';
    while (value) {
        tmp[i++] = digits[value % (uint32_t)base];
        value /= (uint32_t)base;
    }
    int j = 0;
    while (i > 0)
        buf[j++] = tmp[--i];
    buf[j] = 0;
}

int atoi(const char* s) {
    int v = 0;
    int neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (is_digit(*s))
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

/* ctype 子集 */
int is_digit(int c) { return c >= '0' && c <= '9'; }

int is_hex(int c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int is_space(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
