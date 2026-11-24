
#include "buzzer.h"
#include "timer.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>

// ========================================
// CONFIGURACION
// ========================================

#define BUZZER_PIN PD3

// Modulo con reposo en HIGH
#define BUZZER_IDLE_HIGH 1

#define BUZZER_PRESCALER 128UL
#define PAUSA_NOTAS_MS 55UL

// ========================================
// NOTAS
// ========================================

#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784
#define NOTE_A5 880
#define NOTE_C6 1047

typedef struct
{
	uint16_t frecuencia;
	uint16_t duracion;
} Nota;

// ========================================
// MELODIA DE BIENVENIDA
// Secuencia electronica original
// ========================================

static const Nota bienvenida[] PROGMEM = {
	{NOTE_C5, 140},
	{NOTE_E5, 140},
	{NOTE_G5, 180},
	{NOTE_C6, 300},
	{NOTE_G5, 160},
	{NOTE_E5, 160},
	{NOTE_G5, 300}
};

// ========================================
// MELODIA DE VICTORIA
// JINGLE BELLS
// ========================================

static const Nota victoria[] PROGMEM = {
	{NOTE_E5, 180},
	{NOTE_E5, 180},
	{NOTE_E5, 360},

	{NOTE_E5, 180},
	{NOTE_E5, 180},
	{NOTE_E5, 360},

	{NOTE_E5, 180},
	{NOTE_G5, 180},
	{NOTE_C5, 240},
	{NOTE_D5, 120},
	{NOTE_E5, 500},

	{NOTE_F5, 180},
	{NOTE_F5, 180},
	{NOTE_F5, 240},
	{NOTE_F5, 180},

	{NOTE_F5, 180},
	{NOTE_E5, 180},
	{NOTE_E5, 180},
	{NOTE_E5, 180},

	{NOTE_E5, 180},
	{NOTE_D5, 180},
	{NOTE_D5, 180},
	{NOTE_E5, 180},
	{NOTE_D5, 360},
	{NOTE_G5, 420}
};

#define TOTAL_BIENVENIDA \
(sizeof(bienvenida) / sizeof(bienvenida[0]))

#define TOTAL_VICTORIA \
(sizeof(victoria) / sizeof(victoria[0]))

// ========================================
// ESTADOS DEL BUZZER
// ========================================

typedef enum
{
	AUDIO_SILENCIO,
	AUDIO_PITIDO,
	AUDIO_BIENVENIDA,
	AUDIO_VICTORIA
} audio_estado_t;

static audio_estado_t audioEstado =
AUDIO_SILENCIO;

static uint8_t indiceNota = 0;
static uint8_t notaSonando = 0;

// Permite continuar bienvenida tras
// un pitido de aviso
static uint8_t continuarBienvenida = 0;

static uint32_t siguienteCambio = 0;

// ========================================
// SILENCIO FISICO
// ========================================

static void silencioFisico(void)
{
	TCCR2A = 0;
	TCCR2B = 0;

	#if BUZZER_IDLE_HIGH
	PORTD |= (1U << BUZZER_PIN);
	#else
	PORTD &= (uint8_t)~(1U << BUZZER_PIN);
	#endif
}

// ========================================
// GENERAR FRECUENCIA CON TIMER2
// ========================================

static void iniciarTono(uint16_t frecuencia)
{
	if (frecuencia == 0)
	{
		silencioFisico();
		return;
	}

	// Phase Correct PWM:
	// frecuencia = 16MHz / (2 * 128 * TOP)

	uint32_t top =
	16000000UL /
	(2UL * BUZZER_PRESCALER *
	(uint32_t)frecuencia);

	if (top < 4UL)
	{
		top = 4UL;
	}

	if (top > 255UL)
	{
		top = 255UL;
	}

	TCCR2A = 0;
	TCCR2B = 0;

	TCNT2 = 0;

	OCR2A = (uint8_t)top;
	OCR2B = (uint8_t)(top / 2UL);

	// Timer2 modo 5:
	// Phase Correct PWM, TOP OCR2A
	// Salida OC2B en PD3
	TCCR2A =
	(1U << COM2B1) |
	(1U << WGM20);

	// Prescaler 128
	TCCR2B =
	(1U << WGM22) |
	(1U << CS22) |
	(1U << CS20);
}

// ========================================
// LEER NOTA DE PROGMEM
// ========================================

static void iniciarNota(uint32_t ahora)
{
	uint16_t frecuencia;
	uint16_t duracion;

	if (audioEstado == AUDIO_BIENVENIDA)
	{
		frecuencia = pgm_read_word(
		&bienvenida[indiceNota].frecuencia
		);

		duracion = pgm_read_word(
		&bienvenida[indiceNota].duracion
		);
	}
	else if (audioEstado == AUDIO_VICTORIA)
	{
		frecuencia = pgm_read_word(
		&victoria[indiceNota].frecuencia
		);

		duracion = pgm_read_word(
		&victoria[indiceNota].duracion
		);
	}
	else
	{
		return;
	}

	iniciarTono(frecuencia);

	notaSonando = 1;
	siguienteCambio = ahora + duracion;
}

// ========================================
// INICIALIZAR
// ========================================

void BUZZER_Init(void)
{
	#if BUZZER_IDLE_HIGH
	PORTD |= (1U << BUZZER_PIN);
	#else
	PORTD &= (uint8_t)~(1U << BUZZER_PIN);
	#endif

	DDRD |= (1U << BUZZER_PIN);

	audioEstado = AUDIO_SILENCIO;
	indiceNota = 0;
	notaSonando = 0;

	continuarBienvenida = 0;
	siguienteCambio = 0;

	silencioFisico();
}

// ========================================
// DETENER TODO
// ========================================

void BUZZER_Stop(void)
{
	audioEstado = AUDIO_SILENCIO;

	indiceNota = 0;
	notaSonando = 0;

	continuarBienvenida = 0;
	siguienteCambio = 0;

	silencioFisico();
}

// ========================================
// PITIDO NORMAL
// ========================================

void BUZZER_Beep(
uint16_t frequency_hz,
uint16_t duration_ms)
{
	// Si estamos en bienvenida,
	// la retomaremos despues del aviso
	uint8_t reanudar =
	(audioEstado == AUDIO_BIENVENIDA);

	BUZZER_Stop();

	if (
	frequency_hz == 0 ||
	duration_ms == 0
	)
	{
		return;
	}

	iniciarTono(frequency_hz);

	audioEstado = AUDIO_PITIDO;
	continuarBienvenida = reanudar;

	siguienteCambio =
	TIMER_Millis() + duration_ms;
}

// ========================================
// INICIAR BIENVENIDA
// ========================================

void BUZZER_PlayWelcome(void)
{
	BUZZER_Stop();

	audioEstado = AUDIO_BIENVENIDA;
	indiceNota = 0;

	iniciarNota(TIMER_Millis());
}

// ========================================
// INICIAR VICTORIA
// ========================================

void BUZZER_PlayVictory(void)
{
	BUZZER_Stop();

	audioEstado = AUDIO_VICTORIA;
	indiceNota = 0;

	iniciarNota(TIMER_Millis());
}

// ========================================
// ACTUALIZACION NO BLOQUEANTE
// ========================================

void BUZZER_Task(void)
{
	uint32_t ahora = TIMER_Millis();

	if (audioEstado == AUDIO_SILENCIO)
	{
		return;
	}

	if (
	(int32_t)(
	ahora - siguienteCambio
	) < 0
	)
	{
		return;
	}

	// ------------------------------------
	// FINALIZAR PITIDO
	// ------------------------------------

	if (audioEstado == AUDIO_PITIDO)
	{
		silencioFisico();

		if (continuarBienvenida)
		{
			continuarBienvenida = 0;

			audioEstado = AUDIO_BIENVENIDA;
			indiceNota = 0;
			notaSonando = 0;

			siguienteCambio = ahora + 200UL;
		}
		else
		{
			audioEstado = AUDIO_SILENCIO;
		}

		return;
	}

	// ------------------------------------
	// TERMINO UNA NOTA
	// ------------------------------------

	if (notaSonando)
	{
		silencioFisico();

		notaSonando = 0;
		indiceNota++;

		siguienteCambio =
		ahora + PAUSA_NOTAS_MS;

		return;
	}

	// ------------------------------------
	// MELODIA DE BIENVENIDA
	// ------------------------------------

	if (audioEstado == AUDIO_BIENVENIDA)
	{
		if (indiceNota >= TOTAL_BIENVENIDA)
		{
			indiceNota = 0;

			// Pausa antes de repetir
			siguienteCambio = ahora + 650UL;

			return;
		}

		iniciarNota(ahora);
		return;
	}

	// ------------------------------------
	// MELODIA DE VICTORIA
	// ------------------------------------

	if (audioEstado == AUDIO_VICTORIA)
	{
		if (indiceNota >= TOTAL_VICTORIA)
		{
			BUZZER_Stop();
			return;
		}

		iniciarNota(ahora);
	}
}
