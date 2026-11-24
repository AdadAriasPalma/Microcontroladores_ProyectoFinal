
#include "max7219.h"
#include <avr/io.h>
#include <stdint.h>

#define MAX_DIN PB3
#define MAX_CS  PB2
#define MAX_CLK PB5

#define REG_DECODE    0x09
#define REG_INTENSITY 0x0A
#define REG_SCANLIMIT 0x0B
#define REG_SHUTDOWN  0x0C
#define REG_TEST      0x0F

#define SCROLL_MS 180UL
#define MAX_MESSAGE 24

static uint8_t scrolling = 0;
static char mensaje[MAX_MESSAGE + 1];
static uint8_t longitud = 0;
static uint8_t desplazamiento = 0;
static uint32_t ultimoMovimiento = 0;

// ========================================
// ENVIAR REGISTRO AL MAX7219
// ========================================

static void MAX7219_Write(
uint8_t registro,
uint8_t dato)
{
	uint16_t paquete =
	((uint16_t)registro << 8) | dato;

	PORTB &= (uint8_t)~(1U << MAX_CS);

	for (uint8_t i = 0; i < 16; i++)
	{
		PORTB &= (uint8_t)~(1U << MAX_CLK);

		if (paquete & 0x8000U)
		{
			PORTB |= (1U << MAX_DIN);
		}
		else
		{
			PORTB &= (uint8_t)~(1U << MAX_DIN);
		}

		PORTB |= (1U << MAX_CLK);

		paquete <<= 1;
	}

	PORTB &= (uint8_t)~(1U << MAX_CLK);
	PORTB |= (1U << MAX_CS);
}

// ========================================
// CARACTERES PARA SIETE SEGMENTOS
// ========================================

// Bits: DP G F E D C B A

static uint8_t caracterSegmentos(char c)
{
	switch (c)
	{
		case '0': return 0x3F;
		case '1': return 0x06;
		case '2': return 0x5B;
		case '3': return 0x4F;
		case '4': return 0x66;
		case '5': return 0x6D;
		case '6': return 0x7D;
		case '7': return 0x07;
		case '8': return 0x7F;
		case '9': return 0x6F;

		case 'A': return 0x77;
		case 'B': return 0x7C;
		case 'C': return 0x39;
		case 'D': return 0x5E;
		case 'E': return 0x79;
		case 'F': return 0x71;
		case 'G': return 0x3D;
		case 'H': return 0x76;
		case 'I': return 0x06;
		case 'J': return 0x1E;
		case 'L': return 0x38;
		case 'N': return 0x54;
		case 'O': return 0x3F;
		case 'P': return 0x73;
		case 'R': return 0x50;
		case 'S': return 0x6D;
		case 'T': return 0x78;
		case 'U': return 0x3E;
		case 'Y': return 0x6E;

		case '-': return 0x40;
		case '_': return 0x08;
		case ' ': return 0x00;

		default: return 0x00;
	}
}

// ========================================
// INICIALIZACION
// ========================================

void MAX7219_Init(void)
{
	DDRB |= (1U << MAX_DIN) |
	(1U << MAX_CS) |
	(1U << MAX_CLK);

	PORTB |= (1U << MAX_CS);

	PORTB &= (uint8_t)~(
	(1U << MAX_DIN) |
	(1U << MAX_CLK)
	);

	scrolling = 0;

	MAX7219_Write(REG_TEST, 0x00);
	MAX7219_Write(REG_SHUTDOWN, 0x01);
	MAX7219_Write(REG_DECODE, 0xFF);
	MAX7219_Write(REG_SCANLIMIT, 0x07);
	MAX7219_Write(REG_INTENSITY, 0x05);

	for (uint8_t i = 1; i <= 8; i++)
	{
		MAX7219_Write(i, 0x0F);
	}
}

// ========================================
// MOSTRAR CRONOMETRO MM.SS
// ========================================

void MAX7219_ShowSeconds(uint16_t seconds)
{
	scrolling = 0;

	MAX7219_Write(REG_DECODE, 0xFF);

	// Limitar a 99 minutos y 59 segundos
	if (seconds > 5999U)
	{
		seconds = 5999U;
	}

	uint8_t minutos =
	(uint8_t)(seconds / 60U);

	uint8_t segundos =
	(uint8_t)(seconds % 60U);

	MAX7219_Write(1, segundos % 10U);
	MAX7219_Write(2, segundos / 10U);

	// Punto decimal en unidades de minutos
	MAX7219_Write(
	3,
	(uint8_t)((minutos % 10U) | 0x80U)
	);

	MAX7219_Write(4, minutos / 10U);

	for (uint8_t i = 5; i <= 8; i++)
	{
		MAX7219_Write(i, 0x0F);
	}
}

// ========================================
// ENCENDIDO Y BRILLO
// ========================================

void MAX7219_SetEnabled(uint8_t enabled)
{
	MAX7219_Write(
	REG_SHUTDOWN,
	enabled ? 0x01 : 0x00
	);
}

void MAX7219_SetBrightness(uint8_t brightness)
{
	if (brightness > 15U)
	{
		brightness = 15U;
	}

	MAX7219_Write(REG_INTENSITY, brightness);
}

// ========================================
// DIBUJAR VENTANA DEL MENSAJE
// ========================================

static void dibujarMensaje(void)
{
	// Cada posicion del mensaje aparece
	// desde la derecha hacia la izquierda.

	for (uint8_t i = 0; i < 8; i++)
	{
		int16_t indice =
		(int16_t)desplazamiento +
		(int16_t)i - 8;

		char caracter = ' ';

		if (
		indice >= 0 &&
		indice < longitud
		)
		{
			caracter = mensaje[indice];
		}

		// Registro 8 = digito izquierdo
		// Registro 1 = digito derecho
		MAX7219_Write(
		(uint8_t)(8U - i),
		caracterSegmentos(caracter)
		);
	}
}

// ========================================
// INICIAR MENSAJE DESPLAZABLE
// ========================================

void MAX7219_StartScroll(const char *message)
{
	longitud = 0;

	if (message != 0)
	{
		while (
		message[longitud] != '\0' &&
		longitud < MAX_MESSAGE
		)
		{
			char c = message[longitud];

			// Convertir minusculas a mayusculas
			if (c >= 'a' && c <= 'z')
			{
				c = (char)(c - 'a' + 'A');
			}

			mensaje[longitud] = c;
			longitud++;
		}
	}

	mensaje[longitud] = '\0';

	scrolling = 1;
	desplazamiento = 0;
	ultimoMovimiento = 0;

	// Modo sin decodificacion para letras
	MAX7219_Write(REG_DECODE, 0x00);
	MAX7219_Write(REG_SHUTDOWN, 0x01);

	dibujarMensaje();
}

// ========================================
// ANIMAR MENSAJE
// ========================================

void MAX7219_Task(uint32_t now_ms)
{
	if (!scrolling)
	{
		return;
	}

	if (
	(uint32_t)(
	now_ms - ultimoMovimiento
	) < SCROLL_MS
	)
	{
		return;
	}

	ultimoMovimiento = now_ms;

	desplazamiento++;

	// Reiniciar despues de salir por la izquierda
	if (desplazamiento > (uint8_t)(longitud + 8U))
	{
		desplazamiento = 0;
	}

	dibujarMensaje();
}
