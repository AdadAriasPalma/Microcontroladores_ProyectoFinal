#ifndef GPIO_H_
#define GPIO_H_
#include <avr/io.h>
#include <stdint.h>
typedef enum { GPIO_PORTB, GPIO_PORTC, GPIO_PORTD } gpio_port_t;
typedef enum { GPIO_INPUT=0, GPIO_OUTPUT=1 } gpio_mode_t;
typedef enum { GPIO_LOW=0, GPIO_HIGH=1 } gpio_state_t;
void GPIO_ConfigPin(gpio_port_t port, uint8_t pin, gpio_mode_t mode);
void GPIO_ConfigPort(gpio_port_t port, uint8_t mask);
gpio_state_t GPIO_ReadPin(gpio_port_t port, uint8_t pin);
void GPIO_WritePin(gpio_port_t port, uint8_t pin, gpio_state_t state);
uint8_t GPIO_ReadPort(gpio_port_t port);
void GPIO_WritePort(gpio_port_t port, uint8_t value);
void GPIO_EnablePullUp(gpio_port_t port, uint8_t pin);
#endif
