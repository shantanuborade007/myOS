#pragma once
#include "../include/types.h"

struct Time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
};

void rtc_init();
Time rtc_get_time();
