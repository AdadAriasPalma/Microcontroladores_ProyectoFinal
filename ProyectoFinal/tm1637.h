#ifndef TM1637_H_
#define TM1637_H_
#include <stdint.h>
void TM1637_Init(void);
void TM1637_ShowSeconds(uint16_t seconds);
void TM1637_ShowDashes(void);
void TM1637_Task(void);
#endif
