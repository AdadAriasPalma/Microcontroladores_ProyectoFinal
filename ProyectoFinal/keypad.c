
#include "keypad.h"
#include "timer.h"

#include <avr/io.h>
#include <stdint.h>

// ========================================
// TECLADO DE PROTEUS
// KEYPAD-SMALLCALC
// ========================================

// FILAS:
// A -> D12 -> PB4
// B -> A1  -> PC1
// C -> A2  -> PC2
// D -> A3  -> PC3

// COLUMNAS:
// 1 -> D7 -> PD7
// 2 -> D8 -> PB0
// 3 -> D9 -> PB1
// 4 -> SIN CONECTAR

#define KEYPAD_SCAN_MS 10UL
#define KEYPAD_DEBOUNCE_MS 35UL

// ========================================
// DISTRIBUCION DE TECLAS
// ========================================

// Teclado de Proteus:
//
//  7   8   9   /
//  4   5   6   X
//  1   2   3   -
// ON   0   =   +
//
// ON/C se interpreta como '*'
// '=' se interpreta como '#'
//
// No se utiliza la cuarta columna.


static const char mapa[4][3] = {
	{'1', '2', '3'},
	{'4', '5', '6'},
	{'7', '8', '9'},
	{'*', '0', '#'}
};


// ========================================
// VARIABLES INTERNAS
// ========================================

static char teclaCandidata = 0;
static char teclaEstable = 0;
static char eventoTecla = 0;

static uint32_t ultimoEscaneo = 0;
static uint32_t inicioRebote = 0;

// ========================================
// DESACTIVAR TODAS LAS FILAS
// ========================================

static void filasEnAlto(void)
{
	PORTB |= (1U << PB4);

	PORTC |= (1U << PC1) |
	(1U << PC2) |
	(1U << PC3);
}

// ========================================
// ACTIVAR UNA FILA
// ========================================

static void activarFila(uint8_t fila)
{
	// Primero desactivar todas las filas
	filasEnAlto();

	// Activar solamente una fila en LOW
	switch (fila)
	{
		case 0:
		PORTB &= (uint8_t)~(1U << PB4);
		break;

		case 1:
		PORTC &= (uint8_t)~(1U << PC1);
		break;

		case 2:
		PORTC &= (uint8_t)~(1U << PC2);
		break;

		case 3:
		PORTC &= (uint8_t)~(1U << PC3);
		break;

		default:
		break;
	}
}

// ========================================
// LEER COLUMNAS
// ========================================

static int8_t leerColumna(void)
{
	// Columna 1: D7 / PD7
	if (!(PIND & (1U << PD7)))
	{
		return 0;
	}

	// Columna 2: D8 / PB0
	if (!(PINB & (1U << PB0)))
	{
		return 1;
	}

	// Columna 3: D9 / PB1
	if (!(PINB & (1U << PB1)))
	{
		return 2;
	}

	return -1;
}

// ========================================
// ESCANEAR TECLADO COMPLETO
// ========================================

static char escanearTeclado(void)
{
	char encontrada = 0;

	for (uint8_t fila = 0; fila < 4; fila++)
	{
		activarFila(fila);

		// Permitir que se estabilicen
		// los niveles de las entradas
		__asm__ __volatile__(
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		::: "memory"
		);

		int8_t columna = leerColumna();

		if (columna >= 0)
		{
			encontrada =
			mapa[fila][(uint8_t)columna];

			break;
		}
	}

	// Dejar las cuatro filas en alto
	filasEnAlto();

	return encontrada;
}

// ========================================
// INICIALIZAR TECLADO
// ========================================

void KEYPAD_Init(void)
{
	// ------------------------------------
	// CONFIGURAR FILAS COMO SALIDAS
	// ------------------------------------

	// Fila A -> PB4
	DDRB |= (1U << PB4);

	// Filas B, C y D -> PC1, PC2, PC3
	DDRC |= (1U << PC1) |
	(1U << PC2) |
	(1U << PC3);

	// Todas las filas inicialmente HIGH
	filasEnAlto();

	// ------------------------------------
	// CONFIGURAR COLUMNAS COMO ENTRADAS
	// ------------------------------------

	// Columna 1 -> PD7
	DDRD &= (uint8_t)~(1U << PD7);

	// Columnas 2 y 3 -> PB0 y PB1
	DDRB &= (uint8_t)~(
	(1U << PB0) |
	(1U << PB1)
	);

	// ------------------------------------
	// ACTIVAR PULL-UP INTERNAS
	// ------------------------------------

	PORTD |= (1U << PD7);

	PORTB |= (1U << PB0) |
	(1U << PB1);

	// ------------------------------------
	// REINICIAR VARIABLES
	// ------------------------------------

	teclaCandidata = 0;
	teclaEstable = 0;
	eventoTecla = 0;

	ultimoEscaneo = TIMER_Millis();
	inicioRebote = ultimoEscaneo;
}

// ========================================
// ACTUALIZAR TECLADO
// ========================================

void KEYPAD_Task(void)
{
	uint32_t ahora = TIMER_Millis();

	// Escanear cada 10 ms reales
	if (
	(uint32_t)(
	ahora - ultimoEscaneo
	) < KEYPAD_SCAN_MS
	)
	{
		return;
	}

	ultimoEscaneo = ahora;

	char teclaActual = escanearTeclado();

	// ------------------------------------
	// DETECTAR CAMBIO DE TECLA
	// ------------------------------------

	if (teclaActual != teclaCandidata)
	{
		teclaCandidata = teclaActual;

		inicioRebote = ahora;
	}

	// ------------------------------------
	// ANTIRREBOTE DE 35 ms
	// ------------------------------------

	if (
	teclaCandidata != teclaEstable &&
	(uint32_t)(
	ahora - inicioRebote
	) >= KEYPAD_DEBOUNCE_MS
	)
	{
		teclaEstable = teclaCandidata;

		// Registrar solamente pulsaciones
		// No registrar liberaciones
		if (teclaEstable != 0)
		{
			eventoTecla = teclaEstable;
		}
	}
}

// ========================================
// OBTENER TECLA PRESIONADA
// ========================================

char KEYPAD_GetKey(void)
{
	char tecla = eventoTecla;

	// Consumir el evento
	eventoTecla = 0;

	return tecla;
}
