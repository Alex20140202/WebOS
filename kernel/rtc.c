/* WebOS - CMOS RTC：读取实时时钟日期/时间 */
#include "kernel.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t rtc_seconds, rtc_minutes, rtc_hours;
static uint8_t rtc_day, rtc_month;
static uint16_t rtc_year;
static int rtc_ready_flag;
static int rtc_binary;
static int rtc_24h;

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd(uint8_t v) {
    return (uint8_t)((v & 0x0F) + (v >> 4) * 10);
}

static uint8_t conv(uint8_t v) {
    return rtc_binary ? v : bcd(v);
}

void rtc_init(void) {
    /* 等待 RTC 更新进行中标志 */
    while (cmos_read(0x0A) & 0x80)
        ;
    /* 寄存器 B：bit1 = 二进制编码, bit2 = 24 小时制 */
    uint8_t regb = cmos_read(0x0B);
    rtc_binary = regb & 0x04;
    rtc_24h    = regb & 0x02;

    rtc_seconds = conv(cmos_read(0x00));
    rtc_minutes = conv(cmos_read(0x02));
    rtc_hours   = conv(cmos_read(0x04));
    rtc_day     = conv(cmos_read(0x07));
    rtc_month   = conv(cmos_read(0x08));
    rtc_year    = conv(cmos_read(0x09));
    rtc_ready_flag = 1;
}

int rtc_ready(void) { return rtc_ready_flag; }

void rtc_get(uint32_t* year, uint32_t* month, uint32_t* day,
             uint32_t* hour, uint32_t* minute, uint32_t* second) {
    if (!rtc_ready_flag) {
        if (year)   *year = 0;
        if (month)  *month = 0;
        if (day)    *day = 0;
        if (hour)   *hour = 0;
        if (minute) *minute = 0;
        if (second) *second = 0;
        return;
    }
    rtc_init();     /* 读最新值 */
    if (year)   *year = (uint32_t)rtc_year + 2000;
    if (month)  *month = rtc_month;
    if (day)    *day = rtc_day;
    if (hour)   *hour = rtc_hours;
    if (minute) *minute = rtc_minutes;
    if (second) *second = rtc_seconds;
}

void rtc_print_time(void) {
    if (!rtc_ready_flag) {
        printk("rtc not ready\n");
        return;
    }
    rtc_init();
    printk("20%u-%02u-%02u %02u:%02u:%02u UTC\n",
           rtc_year, rtc_month, rtc_day,
           rtc_hours, rtc_minutes, rtc_seconds);
}
