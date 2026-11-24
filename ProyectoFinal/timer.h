
#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>

// Inicializar Timer1 en modo CTC
// Base de tiempo de 1 milisegundo
void TIMER_Init(void);

// Obtener milisegundos reales
uint32_t TIMER_Millis(void);

// Obtener milisegundos del juego
// Puede avanzar al doble de velocidad
uint32_t TIMER_GameMillis(void);

// Controlar aceleracion del reloj del juego
// 0 = velocidad normal
// 1 = velocidad doble
void TIMER_SetAccelerated(uint8_t enabled);

#endif
