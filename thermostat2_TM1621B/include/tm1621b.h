#ifndef TM1621B_H
#define TM1621B_H

#include <stdbool.h>
#include <stdint.h>

#define TM1621B_SEG_COUNT 32u
#define TM1621B_COM_COUNT  4u

void tm1621b_init(void);
void tm1621b_clear(void);
void tm1621b_set(uint8_t seg, uint8_t com, bool on);
void tm1621b_flush(void);

#endif
