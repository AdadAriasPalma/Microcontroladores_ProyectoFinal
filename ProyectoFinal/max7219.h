
#ifndef MAX7219_H_
#define MAX7219_H_

#include <stdint.h>

void MAX7219_Init(void);

void MAX7219_ShowSeconds(uint16_t seconds);

void MAX7219_SetEnabled(uint8_t enabled);

void MAX7219_SetBrightness(uint8_t brightness);

// Iniciar mensaje desplazable
void MAX7219_StartScroll(const char *message);

// Actualizar animacion con tiempo real
void MAX7219_Task(uint32_t now_ms);

#endif
