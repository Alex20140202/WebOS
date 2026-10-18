/* WebOS - PS/2 键盘驱动：中断驱动 + 环形缓冲，支持 shift/caps/特殊键 */
#include "kernel.h"

#define KBD_DATA    0x60
#define KBD_STATUS  0x64

#define BUFFER_SIZE 128

struct key_event {
    uint16_t code;      /* ASCII 或 special_key */
};

static volatile struct key_event buffer[BUFFER_SIZE];
static volatile int head;
static volatile int tail;

/* set 1 扫描码 -> ASCII */
static const char sc_ascii_low[] = {
    0, 0x1B, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
};
static const char sc_ascii_up[] = {
    0, 0x1B, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','\"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' ',
};

static int shift_down;
static int ctrl_down;
static int caps_lock;
/* 0xE0 前缀标志：下一个 make/break 是扩展键 */
static int ext_pending;

void keyboard_init(void) {
    head = tail = 0;
    shift_down = 0;
    ctrl_down = 0;
    caps_lock = 0;
    ext_pending = 0;
}

static int is_letter(char c) {
    return (c >= 'a' && c <= 'z');
}

static void push_key(uint16_t code) {
    int next = (tail + 1) % BUFFER_SIZE;
    if (next == head)
        return;             /* 缓冲满则丢弃 */
    buffer[tail].code = code;
    tail = next;
}

void keyboard_handle_interrupt(void) {
    uint8_t sc = inb(KBD_DATA);

    if (sc == 0xE0 || sc == 0xE1) {
        ext_pending = 1;
        return;
    }

    int release = (sc & 0x80) != 0;
    uint8_t make = sc & 0x7F;
    int ext = ext_pending;
    ext_pending = 0;

    switch (make) {
    case 0x2A: case 0x36:   /* shift */
        shift_down = !release;
        return;
    case 0x1D:              /* ctrl */
        ctrl_down = !release;
        return;
    case 0x3A:              /* capslock */
        if (!release)
            caps_lock = !caps_lock;
        return;
    case 0x00:
        return;
    default:
        break;
    }

    /* 扩展键（0xE0 前缀） */
    if (ext) {
        if (release)
            return;
        switch (make) {
        case 0x48: push_key(KEY_UP);       return;
        case 0x50: push_key(KEY_DOWN);     return;
        case 0x4B: push_key(KEY_LEFT);     return;
        case 0x4D: push_key(KEY_RIGHT);    return;
        case 0x47: push_key(KEY_HOME);     return;
        case 0x4F: push_key(KEY_END);      return;
        case 0x49: push_key(KEY_PAGEUP);   return;
        case 0x51: push_key(KEY_PAGEDOWN); return;
        case 0x52: push_key(KEY_INSERT);   return;
        case 0x53: push_key(KEY_DELETE);   return;
        case 0x35: push_key('/');          return; /* 小键盘 / */
        case 0x1C: push_key('\n');         return; /* 小键盘回车 */
        case 0x38: return;                          /* AltGr 忽略 */
        default:   return;
        }
    }

    /* F1-F12（非扩展） */
    if (!release && make >= 0x3B && make <= 0x44) {
        push_key((uint16_t)(KEY_F1 + (make - 0x3B)));
        return;
    }
    if (!release && make >= 0x57 && make <= 0x58) {
        push_key((uint16_t)(KEY_F11 + (make - 0x57)));
        return;
    }

    if (make >= sizeof(sc_ascii_low))
        return;

    char c = shift_down ? sc_ascii_up[make] : sc_ascii_low[make];
    if (!c)
        return;
    if (caps_lock && is_letter(c))
        c = (char)(c - 'a' + 'A');
    if (ctrl_down && is_letter(c))
        c = (char)(c - 'a' + 1);    /* ctrl+a => 0x01 */

    if (release)
        return;
    push_key((uint16_t)(unsigned char)c);
}

int keyboard_has_key(void) {
    return head != tail;
}

int keyboard_getkey_nonblock(void) {
    if (!keyboard_has_key())
        return KEY_NONE;
    uint16_t c = buffer[head].code;
    head = (head + 1) % BUFFER_SIZE;
    return c;
}

int keyboard_getkey(void) {
    for (;;) {
        int c = keyboard_getkey_nonblock();
        if (c != KEY_NONE)
            return c;
        hlt();              /* 等中断唤醒，省电 */
    }
}

/* 兼容旧接口：阻塞读一个 ASCII 字符 */
char keyboard_getchar(void) {
    for (;;) {
        int k = keyboard_getkey();
        if (k < 0x100)
            return (char)k;
    }
}
