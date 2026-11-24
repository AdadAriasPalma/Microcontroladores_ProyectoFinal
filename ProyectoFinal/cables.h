#ifndef CABLES_H_
#define CABLES_H_
#include <stdint.h>
void CABLES_Init(void);
void CABLES_Task(void);
uint8_t CABLES_AllConnected(void);
uint8_t CABLES_NextDisconnected(void); // 1..4, 0 si no hay evento
void CABLES_ClearEvents(void);
#endif
