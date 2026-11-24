
#include "timer.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// ========================================
// VARIABLES DE TIEMPO
// ========================================

// Milisegundos reales transcurridos
static volatile uint32_t tiempoReal = 0;

// Milisegundos del juego
static volatile uint32_t tiempoJuego = 0;

// 0 = velocidad normal
// 1 = velocidad doble
static volatile uint8_t acelerado = 0;

// ========================================
// INTERRUPCION DE TIMER1
// ========================================

ISR(TIMER1_COMPA_vect)
{
	// Siempre avanza 1 ms real
	tiempoReal++;

	// El reloj del juego puede avanzar 2 ms
	if (acelerado)
	{
		tiempoJuego += 2UL;
	}
	else
	{
		tiempoJuego++;
	}
}

// ========================================
// INICIALIZAR TIMER1
// ========================================

void TIMER_Init(void)
{
	uint8_t estadoInterrupciones = SREG;

	cli();

	tiempoReal = 0;
	tiempoJuego = 0;
	acelerado = 0;

	// Detener Timer1 mientras configuramos
	TCCR1A = 0;
	TCCR1B = 0;

	// Reiniciar contador
	TCNT1 = 0;

	// 16 MHz / 64 = 250000 Hz
	// 250 cuentas = 1 ms
	// OCR1A = 250 - 1 = 249
	OCR1A = 249;

	// Limpiar bandera de interrupcion
	TIFR1 = (1 << OCF1A);

	// Activar interrupcion por comparacion A
	TIMSK1 = (1 << OCIE1A);

	// Modo CTC, prescaler de 64
	TCCR1B = (1 << WGM12) |
	(1 << CS11) |
	(1 << CS10);

	// Restaurar estado de interrupciones
	SREG = estadoInterrupciones;
}

// ========================================
// OBTENER TIEMPO REAL
// ========================================

uint32_t TIMER_Millis(void)
{
	uint32_t copia;

	uint8_t estadoInterrupciones = SREG;

	cli();

	copia = tiempoReal;

	SREG = estadoInterrupciones;

	return copia;
}

// ========================================
// OBTENER TIEMPO DEL JUEGO
// ========================================

uint32_t TIMER_GameMillis(void)
{
	uint32_t copia;

	uint8_t estadoInterrupciones = SREG;

	cli();

	copia = tiempoJuego;

	SREG = estadoInterrupciones;

	return copia;
}

// ========================================
// ACTIVAR O DESACTIVAR ACELERACION
// ========================================

void TIMER_SetAccelerated(uint8_t enabled)
{
	uint8_t estadoInterrupciones = SREG;

	cli();

	if (enabled)
	{
		acelerado = 1;
	}
	else
	{
		acelerado = 0;
	}

	SREG = estadoInterrupciones;
}
