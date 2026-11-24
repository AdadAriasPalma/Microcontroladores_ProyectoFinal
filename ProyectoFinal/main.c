
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"
#include "i2c.h"
#include "lcd.h"
#include "max7219.h"
#include "keypad.h"
#include "adc.h"
#include "buzzer.h"
#include "cables.h"
#include "juego.h"

// ========================================
// PROYECTO FINAL
// SIMULADOR DE DESACTIVACION
// ATmega328P - 16 MHz
// ========================================

int main(void)
{
	// Desactivar interrupciones
	cli();

	// Timer1: base de tiempo de 1 ms
	TIMER_Init();

	// Bus I2C para LCD y teclado
	I2C_Init();

	// LCD 20x4
	LCD_Init();

	// Display MAX7219
	MAX7219_Init();

	// Teclado 4x4 mediante PCF8574
	KEYPAD_Init();

	// Potenciometro ADC0
	ADC_Init();

	// Buzzer en D3
	BUZZER_Init();

	// Cables D4, D5, D6 y D2
	CABLES_Init();

	// Inicializar juego
	JUEGO_Init();

	// Activar interrupciones globales
	sei();

	// Bucle principal
	
	while (1)
	{
		// Actualizar lectura del potenciometro
		ADC_Task();

		// Actualizar teclado I2C
		KEYPAD_Task();

		// Detectar desconexion de cables
		CABLES_Task();

		// Actualizar buzzer
		BUZZER_Task();

		// Ejecutar logica del juego
		JUEGO_Update();

		// Actualizar pantalla LCD
		LCD_Task();
	}


	return 0;
}
