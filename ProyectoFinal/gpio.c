#include "gpio.h"
static volatile uint8_t *ddr(gpio_port_t port) {
    if (port==GPIO_PORTB) return &DDRB;
    if (port==GPIO_PORTC) return &DDRC;
    return &DDRD;
}
static volatile uint8_t *output(gpio_port_t port) {
    if (port==GPIO_PORTB) return &PORTB;
    if (port==GPIO_PORTC) return &PORTC;
    return &PORTD;
}
static volatile uint8_t *input(gpio_port_t port) {
    if (port==GPIO_PORTB) return &PINB;
    if (port==GPIO_PORTC) return &PINC;
    return &PIND;
}
void GPIO_ConfigPin(gpio_port_t port, uint8_t pin, gpio_mode_t mode) {
    if (pin>7) return;
    if (mode==GPIO_OUTPUT) *ddr(port)|=(uint8_t)(1U<<pin);
    else *ddr(port)&=(uint8_t)~(1U<<pin);
}
void GPIO_ConfigPort(gpio_port_t port, uint8_t mask) { *ddr(port)=mask; }
gpio_state_t GPIO_ReadPin(gpio_port_t port, uint8_t pin) {
    if (pin>7) return GPIO_LOW;
    return (*input(port)&(1U<<pin))?GPIO_HIGH:GPIO_LOW;
}
void GPIO_WritePin(gpio_port_t port, uint8_t pin, gpio_state_t state) {
    if (pin>7) return;
    if (state==GPIO_HIGH) *output(port)|=(uint8_t)(1U<<pin);
    else *output(port)&=(uint8_t)~(1U<<pin);
}
uint8_t GPIO_ReadPort(gpio_port_t port) { return *input(port); }
void GPIO_WritePort(gpio_port_t port, uint8_t value) { *output(port)=value; }
void GPIO_EnablePullUp(gpio_port_t port, uint8_t pin) {
    GPIO_ConfigPin(port,pin,GPIO_INPUT);
    GPIO_WritePin(port,pin,GPIO_HIGH);
}
