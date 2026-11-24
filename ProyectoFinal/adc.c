#include "adc.h"
#include "timer.h"
#include <avr/io.h>
static uint8_t value=0;
static uint32_t next_measure=0;
void ADC_Init(void) {
    ADMUX=(1U<<REFS0); // AVCC referencia, ADC0/PC0
    ADCSRA=(1U<<ADEN)|(1U<<ADPS2)|(1U<<ADPS1)|(1U<<ADPS0); // divisor 128
    next_measure=0;
}
void ADC_Task(void) {
    uint32_t now=TIMER_Millis();
    if(ADCSRA&(1U<<ADSC)) return;
    static uint8_t running=0;
    if(running) {
        uint16_t raw=ADC;
        value=(uint8_t)(((uint32_t)raw*100U+511U)/1023U);
        running=0;
    }
    if((int32_t)(now-next_measure)>=0) {
        ADCSRA|=(1U<<ADSC);
        running=1;
        next_measure=now+10;
    }
}
uint8_t ADC_GetPercent(void) { return (uint8_t)(100U - value); }
