#include "tm1637.h"
#include "gpio.h"
#include "timer.h"
#include <avr/io.h>
#define CLK_BIT 5   // PB5: Arduino D13
#define DIO_BIT 4   // PB4: Arduino D12
static uint8_t digits[4]={0,0,0,0};
static uint8_t dirty=1;
static uint32_t last_flush=0;
static const uint8_t segs[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
static void pause_bus(void) {
    // Pausa muy corta exigida por el protocolo (~2 us a 16 MHz).
    for(volatile uint8_t i=0;i<20;i++) { __asm__ __volatile__("nop"); }
}
static void release_line(uint8_t pin) {
    PORTB|=(1U<<pin); // pull-up de respaldo
    DDRB&=(uint8_t)~(1U<<pin); // alta impedancia
}
static void low_line(uint8_t pin) {
    PORTB&=(uint8_t)~(1U<<pin);
    DDRB|=(1U<<pin);
}
static void start(void) { release_line(DIO_BIT); release_line(CLK_BIT); pause_bus(); low_line(DIO_BIT); pause_bus(); low_line(CLK_BIT); }
static void stop(void) { low_line(CLK_BIT); low_line(DIO_BIT); pause_bus(); release_line(CLK_BIT); pause_bus(); release_line(DIO_BIT); pause_bus(); }
static void send(uint8_t b) {
    for(uint8_t i=0;i<8;i++) {
        low_line(CLK_BIT);
        if (b&1U) release_line(DIO_BIT); else low_line(DIO_BIT);
        pause_bus();
        release_line(CLK_BIT); pause_bus();
        b>>=1;
    }
    low_line(CLK_BIT); release_line(DIO_BIT); pause_bus();
    release_line(CLK_BIT); pause_bus(); // ACK del TM1637 (no evaluado)
    low_line(CLK_BIT);
}
void TM1637_Init(void) { release_line(CLK_BIT); release_line(DIO_BIT); dirty=1; }
void TM1637_ShowSeconds(uint16_t seconds) {
    if(seconds>5999) seconds=5999;
    uint16_t mm=seconds/60, ss=seconds%60;
    digits[0]=segs[(mm/10)%10];
    digits[1]=(uint8_t)(segs[mm%10]|0x80); // colon/DP central segun modulo
    digits[2]=segs[ss/10];
    digits[3]=segs[ss%10];
    dirty=1;
}
void TM1637_ShowDashes(void) { for(uint8_t i=0;i<4;i++)digits[i]=0x40; dirty=1; }
void TM1637_Task(void) {
    uint32_t now=TIMER_Millis();
    if(!dirty || (uint32_t)(now-last_flush)<30) return;
    last_flush=now;
    start(); send(0x40); stop(); // Escritura automatica
    start(); send(0xC0);
    for(uint8_t i=0;i<4;i++)send(digits[i]);
    stop();
    start(); send(0x8F); stop(); // encender, brillo maximo
    dirty=0;
}
