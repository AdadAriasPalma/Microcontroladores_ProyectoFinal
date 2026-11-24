
#include "cables.h"
#include "timer.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdint.h>

// ========================================
// CONFIGURACION DE LOS CABLES
// ========================================

// Cable 1: PD4 / Arduino D4
// Cable 2: PD5 / Arduino D5
// Cable 3: PD6 / Arduino D6
// Cable 4: PD2 / Arduino D2 / INT0

#define TOTAL_CABLES 4
#define ANTIRREBOTE_MS 25UL

static const uint8_t pines[TOTAL_CABLES] = {
	PD4,
	PD5,
	PD6,
	PD2
};

// ========================================
// VARIABLES INTERNAS
// ========================================

// 0 = conectado a GND
// 1 = desconectado (pull-up)

static uint8_t estadoEstable[TOTAL_CABLES];
static uint8_t estadoCandidato[TOTAL_CABLES];

static uint32_t tiempoCambio[TOTAL_CABLES];

// Eventos pendientes, uno por cable
static uint8_t eventosPendientes = 0;

// Bandera de interrupcion del cable 4
static volatile uint8_t eventoINT0 = 0;

// ========================================
// INTERRUPCION EXTERNA INT0
// ========================================

ISR(INT0_vect)
{
	// Se detecto un flanco ascendente
	// en el pin PD2 (cable 4)
	eventoINT0 = 1;
}

// ========================================
// LEER ESTADO DE UN CABLE
// ========================================

static uint8_t leerCable(uint8_t indice)
{
	if (PIND & (1U << pines[indice]))
	{
		return 1;
	}

	return 0;
}

// ========================================
// INICIALIZAR LOS CUATRO CABLES
// ========================================

void CABLES_Init(void)
{
	uint32_t ahora = TIMER_Millis();

	for (uint8_t i = 0; i < TOTAL_CABLES; i++)
	{
		// Configurar pin como entrada
		DDRD &= (uint8_t)~(1U << pines[i]);

		// Activar resistencia pull-up interna
		PORTD |= (1U << pines[i]);

		// Registrar estado inicial
		estadoEstable[i] = leerCable(i);
		estadoCandidato[i] = estadoEstable[i];

		tiempoCambio[i] = ahora;
	}

	eventosPendientes = 0;
	eventoINT0 = 0;

	// INT0 en flanco ascendente:
	// LOW -> HIGH
	EICRA = (uint8_t)(
	(EICRA & (uint8_t)~(
	(1U << ISC01) |
	(1U << ISC00)
	)) |
	(1U << ISC01) |
	(1U << ISC00)
	);

	// Limpiar interrupcion pendiente
	EIFR = (1U << INTF0);

	// Habilitar INT0
	EIMSK |= (1U << INT0);
}

// ========================================
// VERIFICAR SI TODOS ESTAN CONECTADOS
// ========================================

uint8_t CABLES_AllConnected(void)
{
	for (uint8_t i = 0; i < TOTAL_CABLES; i++)
	{
		if (leerCable(i) != 0)
		{
			return 0;
		}
	}

	return 1;
}

// ========================================
// BORRAR EVENTOS ANTERIORES
// ========================================

void CABLES_ClearEvents(void)
{
	uint32_t ahora = TIMER_Millis();

	eventosPendientes = 0;

	for (uint8_t i = 0; i < TOTAL_CABLES; i++)
	{
		estadoEstable[i] = leerCable(i);
		estadoCandidato[i] = estadoEstable[i];

		tiempoCambio[i] = ahora;
	}

	// Borrar bandera INT0 de forma segura
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		eventoINT0 = 0;
		EIFR = (1U << INTF0);
	}
}

// ========================================
// REVISAR CAMBIOS EN LOS CABLES
// ========================================

void CABLES_Task(void)
{
	uint32_t ahora = TIMER_Millis();

	for (uint8_t i = 0; i < TOTAL_CABLES; i++)
	{
		uint8_t lectura = leerCable(i);

		// Detectar un posible cambio
		if (lectura != estadoCandidato[i])
		{
			estadoCandidato[i] = lectura;
			tiempoCambio[i] = ahora;
		}

		// Confirmar cambio tras 25 ms
		if (
		lectura != estadoEstable[i] &&
		(uint32_t)(
		ahora - tiempoCambio[i]
		) >= ANTIRREBOTE_MS
		)
		{
			estadoEstable[i] = lectura;

			// Solo generar evento al desconectar
			if (lectura == 1)
			{
				if (i == 3)
				{
					// Cable 4: confirmar INT0
					uint8_t interrupcionDetectada;

					ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
					{
						interrupcionDetectada = eventoINT0;
						eventoINT0 = 0;
					}

					if (interrupcionDetectada)
					{
						eventosPendientes |= (1U << i);
					}
				}
				else
				{
					// Cables 1, 2 y 3
					eventosPendientes |= (1U << i);
				}
			}
		}
	}
}

// ========================================
// OBTENER SIGUIENTE CABLE DESCONECTADO
// ========================================

uint8_t CABLES_NextDisconnected(void)
{
	for (uint8_t i = 0; i < TOTAL_CABLES; i++)
	{
		if (eventosPendientes & (1U << i))
		{
			// Consumir evento
			eventosPendientes &=
			(uint8_t)~(1U << i);

			// Regresar numero de cable: 1 a 4
			return (uint8_t)(i + 1U);
		}
	}

	// Ninguna desconexion nueva
	return 0;
}
