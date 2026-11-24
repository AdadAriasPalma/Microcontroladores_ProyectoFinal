
#include "lcd.h"
#include "i2c.h"
#include "timer.h"

#include <stdint.h>

// ========================================
// CONEXIONES DEL PCF8574 A LA LCD
// ========================================

#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_EN 0x04
#define LCD_BL 0x08

#define LCD_TOTAL (LCD_COLS * LCD_ROWS)

// Direcciones DDRAM para LCD 20x4
static const uint8_t direcciones[4] = {
	0x80,
	0xC0,
	0x94,
	0xD4
};

// ========================================
// VARIABLES INTERNAS
// ========================================

static char contenido[LCD_TOTAL];

static uint8_t lista = 0;
static uint8_t pasoInicio = 0;
static uint8_t posicion = 0;
static uint8_t actualizar = 0;
static uint8_t retroiluminacion = LCD_BL;

static uint32_t siguienteInicio = 0;
static uint32_t ultimaActualizacion = 0;

// ========================================
// ENVIAR DATOS A LA LCD
// ========================================

// Enviar cuatro bits
static void LCD_Nibble(uint8_t datos, uint8_t rs)
{
	uint8_t valor =
	(datos & 0xF0) | retroiluminacion;

	if (rs)
	{
		valor |= LCD_RS;
	}

	// RW permanece en 0: escritura
	I2C_WriteByte(LCD_I2C_ADDRESS, valor);

	// Activar ENABLE
	I2C_WriteByte(
	LCD_I2C_ADDRESS,
	valor | LCD_EN
	);

	// Desactivar ENABLE
	I2C_WriteByte(LCD_I2C_ADDRESS, valor);
}

// Enviar ocho bits
static void LCD_Byte(uint8_t dato, uint8_t rs)
{
	LCD_Nibble(dato & 0xF0, rs);

	LCD_Nibble(
	(uint8_t)(dato << 4),
	rs
	);
}

// Enviar comando
static void LCD_Comando(uint8_t comando)
{
	LCD_Byte(comando, 0);
}

// Enviar caracter
static void LCD_Caracter(uint8_t caracter)
{
	LCD_Byte(caracter, 1);
}

// ========================================
// INICIALIZAR LCD
// ========================================

void LCD_Init(void)
{
	for (uint8_t i = 0; i < LCD_TOTAL; i++)
	{
		contenido[i] = ' ';
	}

	lista = 0;
	pasoInicio = 0;
	posicion = 0;
	actualizar = 1;

	retroiluminacion = LCD_BL;

	ultimaActualizacion = 0;

	// Esperar 50 ms despues de iniciar
	siguienteInicio = TIMER_Millis() + 50UL;
}

// ========================================
// CONSULTAR ESTADO
// ========================================

uint8_t LCD_IsReady(void)
{
	return lista;
}

// ========================================
// MOSTRAR CUATRO LINEAS
// ========================================

void LCD_SetText4(
const char *line1,
const char *line2,
const char *line3,
const char *line4)
{
	const char *lineas[4] = {
		line1,
		line2,
		line3,
		line4
	};

	for (uint8_t fila = 0; fila < 4; fila++)
	{
		uint8_t longitud = 0;

		// Calcular longitud maxima de 20
		if (lineas[fila] != 0)
		{
			while (
			longitud < LCD_COLS &&
			lineas[fila][longitud] != '\0'
			)
			{
				longitud++;
			}
		}

		// Centrar texto horizontalmente
		uint8_t inicio =
		(LCD_COLS - longitud) / 2;

		for (uint8_t col = 0; col < LCD_COLS; col++)
		{
			char caracter = ' ';

			if (
			lineas[fila] != 0 &&
			col >= inicio &&
			col < (uint8_t)(inicio + longitud)
			)
			{
				caracter =
				lineas[fila][col - inicio];
			}

			contenido[
			fila * LCD_COLS + col
			] = caracter;
		}
	}

	// Volver a escribir desde la primera fila
	posicion = 0;
	actualizar = 1;
}

// ========================================
// COMPATIBILIDAD CON DOS LINEAS
// ========================================

void LCD_SetText(
const char *line1,
const char *line2)
{
	LCD_SetText4(
	line1,
	line2,
	"",
	""
	);
}

// ========================================
// CONTROL DE RETROILUMINACION
// ========================================

void LCD_SetBacklight(uint8_t enabled)
{
	retroiluminacion =
	enabled ? LCD_BL : 0;

	if (lista)
	{
		// Actualizar el estado del backlight
		I2C_WriteByte(
		LCD_I2C_ADDRESS,
		retroiluminacion
		);
	}
}

// ========================================
// ACTUALIZACION NO BLOQUEANTE
// ========================================

void LCD_Task(void)
{
	uint32_t ahora = TIMER_Millis();

	// ------------------------------------
	// SECUENCIA DE INICIALIZACION
	// ------------------------------------

	if (!lista)
	{
		if (
		(int32_t)(
		ahora - siguienteInicio
		) < 0
		)
		{
			return;
		}

		switch (pasoInicio++)
		{
			case 0:
			case 1:
			case 2:
			LCD_Nibble(0x30, 0);
			siguienteInicio = ahora + 5UL;
			break;

			case 3:
			LCD_Nibble(0x20, 0);
			siguienteInicio = ahora + 2UL;
			break;

			case 4:
			// Modo 4 bits, 2 lineas internas
			// Compatible con LCD 20x4
			LCD_Comando(0x28);
			siguienteInicio = ahora + 2UL;
			break;

			case 5:
			// Display apagado
			LCD_Comando(0x08);
			siguienteInicio = ahora + 2UL;
			break;

			case 6:
			// Limpiar pantalla
			LCD_Comando(0x01);
			siguienteInicio = ahora + 3UL;
			break;

			case 7:
			// Incrementar cursor
			LCD_Comando(0x06);
			siguienteInicio = ahora + 2UL;
			break;

			case 8:
			// Encender display, sin cursor
			LCD_Comando(0x0C);
			siguienteInicio = ahora + 2UL;
			break;

			default:
			lista = 1;
			posicion = 0;
			ultimaActualizacion = ahora;
			actualizar = 1;
			break;
		}

		return;
	}

	// ------------------------------------
	// ACTUALIZAR CONTENIDO
	// ------------------------------------

	if (!actualizar)
	{
		return;
	}

	// Actualizar progresivamente
	if (
	(uint32_t)(
	ahora - ultimaActualizacion
	) < 2UL
	)
	{
		return;
	}

	ultimaActualizacion = ahora;

	// Cambiar direccion al comenzar
	// cada una de las cuatro filas
	if ((posicion % LCD_COLS) == 0)
	{
		uint8_t fila =
		posicion / LCD_COLS;

		LCD_Comando(direcciones[fila]);
	}

	LCD_Caracter(
	(uint8_t)contenido[posicion]
	);

	posicion++;

	if (posicion >= LCD_TOTAL)
	{
		posicion = 0;
		actualizar = 0;
	}
}
