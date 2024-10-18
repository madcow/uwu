#ifndef MAD_COMMON_H
#define MAD_COMMON_H

#include <stddef.h>
#include <stdbool.h>

#define DDR(p)     DDR ## p
#define PIN(p)     PIN ## p
#define PORT(p)    PORT ## p
#define BIT(n)     (0x1U << (n))
#define UNUSED(s)  (void)(s)

typedef unsigned char   byte;
typedef unsigned short  word;

void Info(const char *fmt, ...);

#include <util/delay.h>
#define Sleep(ms) _delay_ms(ms)

#endif // MAD_COMMON_H
