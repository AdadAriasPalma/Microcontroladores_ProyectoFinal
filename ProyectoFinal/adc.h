#ifndef ADC_H_
#define ADC_H_
#include <stdint.h>
void ADC_Init(void);
void ADC_Task(void);
uint8_t ADC_GetPercent(void);
#endif
