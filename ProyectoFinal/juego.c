
#include "juego.h"
#include "timer.h"
#include "lcd.h"
#include "max7219.h"
#include "keypad.h"
#include "adc.h"
#include "buzzer.h"
#include "cables.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========================================
// CONFIGURACION DEL JUEGO
// ========================================

// Cinco minutos iniciales
#define START_SECONDS 300U

// Tiempo para etapa de cables
#define WIRE_SECONDS 30U

// Penalizacion por error
#define ERROR_PENALTY 15U

// Cerradura del potenciometro
#define POT_TOLERANCE 3
#define POT_REVERSE_MARGIN 6
#define POT_STABLE_MS 2000UL
#define POT_MIN_DISTANCE 15

// ========================================
// ESTADOS DEL JUEGO
// ========================================

typedef enum
{
	WAIT_SLEEP,
	WAIT_START,
	CODE,
	STABILIZE,
	WAIT_CABLES,
	CUT_CABLES,
	WIN,
	LOSE
} game_state_t;

static game_state_t estado = WAIT_SLEEP;

// ========================================
// TIEMPO
// ========================================

static uint16_t tiempoRestante = START_SECONDS;

static uint32_t ultimoSegundo = 0;
static uint32_t ultimaPantalla = 0;
static uint32_t errorHasta = 0;

// ========================================
// PIN
// ========================================

static uint8_t posicionPIN = 0;

static char respuestas[4];
static char instrucciones[4][21];
static char operaciones[4][21];

// ========================================
// CERRADURA
// ========================================

static uint8_t medicion = 0;
static uint8_t objetivo = 40;

// Posicion desde la que comienza cada giro
static uint8_t posicionInicial = 0;

// Mayor o menor posicion alcanzada
static uint8_t extremoAlcanzado = 0;

// 1 = debe aumentar
// -1 = debe disminuir
static int8_t direccion = 1;

// Medicion dentro de tolerancia
static uint8_t estabilidadIniciada = 0;
static uint32_t inicioEstabilidad = 0;

// Mensaje temporal despues de fallar
static uint32_t falloPotHasta = 0;

// ========================================
// CABLES
// ========================================

static uint8_t ordenCables[4] = {
	1, 2, 3, 4
};

static uint8_t posicionCable = 0;

// ========================================
// EFECTOS
// ========================================

static uint8_t parpadeo = 0;
static uint8_t pantallaEncendida = 1;
static uint32_t ultimoParpadeo = 0;

// Generador pseudoaleatorio
static uint32_t semilla = 0x31579UL;

// ========================================
// ALEATORIEDAD
// ========================================

static uint32_t aleatorio(void)
{
	semilla =
	semilla * 1664525UL + 1013904223UL;

	return semilla;
}

static uint8_t aleatorioHasta(uint8_t limite)
{
	if (limite == 0)
	{
		return 0;
	}

	return (uint8_t)(
	(aleatorio() >> 16) % limite
	);
}

// ========================================
// PANTALLA
// ========================================

static void mostrar(
const char *fila1,
const char *fila2,
const char *fila3,
const char *fila4)
{
	LCD_SetText4(
	fila1,
	fila2,
	fila3,
	fila4
	);
}

static void cambiarEstado(game_state_t nuevo)
{
	estado = nuevo;
	ultimaPantalla = TIMER_Millis();
}

static void mostrarInicio(void)
{
	mostrar(
	"SIMULADOR DE BOMBA",
	"DESACTIVA EL SISTEMA",
	"TIEMPO LIMITE: 5 MIN",
	"PULSA # PARA JUGAR"
	);
}

// Reposo inicial: sin musica y LCD con luz apagada.
static void mostrarReposo(void)
{
	// Estado inicial silencioso.
	BUZZER_Stop();

	// Sin texto en el MAX7219 durante el reposo.
	MAX7219_SetEnabled(0);

	// LCD encendida con mensaje legible.
	LCD_SetBacklight(1);
	LCD_SetText4(
	"SIMULADOR DE BOMBA",
	"SISTEMA EN ESPERA",
	"PULSA 0 PARA ENTRAR",
	""
	);
}

// Entrar al menu de bienvenida al pulsar 0.
static void abrirMenu(void)
{
	LCD_SetBacklight(1);
	cambiarEstado(WAIT_START);
	mostrarInicio();
	MAX7219_StartScroll("HOLA JUEGA");
	BUZZER_PlayWelcome();
}

// Aviso de conectores sin insertar.
static void mostrarCablesFaltantes(void)
{
	mostrar(
	"NO SE PUEDE INICIAR",
	"REVISA LOS 4 CABLES",
	"TODOS CONECTADOS",
	"PULSA # AL TERMINAR"
	);
}

// ========================================
// DERROTA
// ========================================

static void perder(uint8_t tiempoAgotado)
{
	TIMER_SetAccelerated(0);

	cambiarEstado(LOSE);

	if (tiempoAgotado)
	{
		tiempoRestante = 0;
	}

	MAX7219_ShowSeconds(tiempoRestante);

	mostrar(
	"FALLO CRITICO",
	tiempoAgotado ?
	"TIEMPO AGOTADO" :
	"ORDEN INCORRECTO",
	"SISTEMA BLOQUEADO",
	"PULSA # REINICIAR"
	);

	BUZZER_Beep(3900, 180);

	parpadeo = 1;
	pantallaEncendida = 1;
	ultimoParpadeo = TIMER_Millis();
}

// ========================================
// VICTORIA
// ========================================

static void ganar(void)
{
	TIMER_SetAccelerated(0);

	cambiarEstado(WIN);

	parpadeo = 0;
	pantallaEncendida = 1;

	LCD_SetBacklight(1);
	MAX7219_SetEnabled(1);

	
	BUZZER_PlayVictory();


	mostrar(
	"OPERACION EXITOSA",
	"DESACTIVACION OK",
	"CIRCUITO SEGURO",
	"PULSA # REINICIAR"
	);
}

// ========================================
// GENERAR LOS CUATRO RETOS DEL PIN
// ========================================

static void prepararRetos(void)
{
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t x;
	uint8_t valor;
	uint8_t unos;
	uint8_t tipo;

	// ------------------------------------
	// RETO 1: ARITMETICA
	// ------------------------------------

	tipo = aleatorioHasta(4);

	a = (uint8_t)(2U + aleatorioHasta(7));
	b = (uint8_t)(2U + aleatorioHasta(7));
	c = (uint8_t)(1U + aleatorioHasta(4));

	if (tipo == 0)
	{
		strcpy(
		instrucciones[0],
		"ULTIMO DIGITO"
		);

		snprintf(
		operaciones[0],
		21,
		"(%u*%u+%u) MOD 10",
		(unsigned)a,
		(unsigned)b,
		(unsigned)c
		);

		respuestas[0] = (char)(
		'0' + ((a * b + c) % 10U)
		);
	}
	else if (tipo == 1)
	{
		strcpy(
		instrucciones[0],
		"POTENCIA Y MODULO"
		);

		snprintf(
		operaciones[0],
		21,
		"(%u^2+%u) MOD 10",
		(unsigned)a,
		(unsigned)b
		);

		respuestas[0] = (char)(
		'0' + ((a * a + b) % 10U)
		);
	}
	else if (tipo == 2)
	{
		strcpy(
		instrucciones[0],
		"USA LOS PARENTESIS"
		);

		snprintf(
		operaciones[0],
		21,
		"(%u+%u)*%u MOD 10",
		(unsigned)a,
		(unsigned)b,
		(unsigned)c
		);

		respuestas[0] = (char)(
		'0' + (((a + b) * c) % 10U)
		);
	}
	else
	{
		strcpy(
		instrucciones[0],
		"MULTIPLICA Y RESTA"
		);

		snprintf(
		operaciones[0],
		21,
		"(%u*%u-%u) MOD 10",
		(unsigned)a,
		(unsigned)b,
		(unsigned)c
		);

		int16_t resultado =
		(int16_t)a * b - c;

		resultado %= 10;

		if (resultado < 0)
		{
			resultado += 10;
		}

		respuestas[0] = (char)(
		'0' + resultado
		);
	}

	// ------------------------------------
	// RETO 2: ALGEBRA
	// ------------------------------------

	x = aleatorioHasta(10);

	a = (uint8_t)(
	1U + aleatorioHasta(8)
	);

	tipo = aleatorioHasta(3);

	strcpy(
	instrucciones[1],
	"DESPEJA LA X"
	);

	if (tipo == 0)
	{
		snprintf(
		operaciones[1],
		21,
		"3X+%u=%u",
		(unsigned)a,
		(unsigned)(3U * x + a)
		);
	}
	else if (tipo == 1)
	{
		snprintf(
		operaciones[1],
		21,
		"2(X+%u)=%u",
		(unsigned)a,
		(unsigned)(2U * (x + a))
		);
	}
	else
	{
		snprintf(
		operaciones[1],
		21,
		"4X+%u=%u",
		(unsigned)a,
		(unsigned)(4U * x + a)
		);
	}

	respuestas[1] = (char)('0' + x);

	// ------------------------------------
	// RETO 3: BINARIO
	// ------------------------------------

	valor = (uint8_t)(aleatorio() >> 16);

	unos = 0;

	for (uint8_t i = 0; i < 8; i++)
	{
		uint8_t bit = (uint8_t)(
		(valor >> (7U - i)) & 1U
		);

		operaciones[2][i] =
		(char)('0' + bit);

		unos = (uint8_t)(unos + bit);
	}

	operaciones[2][8] = '\0';

	tipo = aleatorioHasta(3);

	if (tipo == 0)
	{
		strcpy(
		instrucciones[2],
		"CUENTA BITS EN 1"
		);

		respuestas[2] = (char)(
		'0' + unos
		);
	}
	else if (tipo == 1)
	{
		strcpy(
		instrucciones[2],
		"CUENTA BITS EN 0"
		);

		respuestas[2] = (char)(
		'0' + (8U - unos)
		);
	}
	else
	{
		strcpy(
		instrucciones[2],
		"PARIDAD UNOS 0/1"
		);

		respuestas[2] = (char)(
		'0' + (unos & 1U)
		);
	}

	// ------------------------------------
	// RETO 4: LOGICA DIGITAL
	// ------------------------------------

	a = aleatorioHasta(8);
	b = aleatorioHasta(8);

	tipo = aleatorioHasta(3);

	if (tipo == 0)
	{
		strcpy(
		instrucciones[3],
		"XOR EN DECIMAL"
		);

		snprintf(
		operaciones[3],
		21,
		"%u XOR %u = ?",
		(unsigned)a,
		(unsigned)b
		);

		respuestas[3] = (char)(
		'0' + (a ^ b)
		);
	}
	else if (tipo == 1)
	{
		strcpy(
		instrucciones[3],
		"AND EN DECIMAL"
		);

		snprintf(
		operaciones[3],
		21,
		"%u AND %u = ?",
		(unsigned)a,
		(unsigned)b
		);

		respuestas[3] = (char)(
		'0' + (a & b)
		);
	}
	else
	{
		strcpy(
		instrucciones[3],
		"OR EN DECIMAL"
		);

		snprintf(
		operaciones[3],
		21,
		"%u OR %u = ?",
		(unsigned)a,
		(unsigned)b
		);

		respuestas[3] = (char)(
		'0' + (a | b)
		);
	}
}

// ========================================
// ORDEN ALEATORIO DE CABLES
// ========================================

static void prepararOrdenCables(void)
{
	ordenCables[0] = 1;
	ordenCables[1] = 2;
	ordenCables[2] = 3;
	ordenCables[3] = 4;

	for (int8_t i = 2; i > 0; i--)
	{
		uint8_t j = aleatorioHasta(
		(uint8_t)(i + 1)
		);

		uint8_t temporal = ordenCables[i];

		ordenCables[i] = ordenCables[j];
		ordenCables[j] = temporal;
	}
}

// ========================================
// MOSTRAR RETO
// ========================================

static void mostrarReto(void)
{
	char titulo[21];
	char progreso[21];
	char digitos[5] = "____";

	for (uint8_t i = 0; i < posicionPIN; i++)
	{
		digitos[i] = respuestas[i];
	}

	snprintf(
	titulo,
	sizeof(titulo),
	"ETAPA 1 - RETO %u/4",
	(unsigned)(posicionPIN + 1U)
	);

	snprintf(
	progreso,
	sizeof(progreso),
	"PIN: %s",
	digitos
	);

	mostrar(
	titulo,
	instrucciones[posicionPIN],
	operaciones[posicionPIN],
	progreso
	);
}

// ========================================
// GENERAR OBJETIVO DE CERRADURA
// ========================================

static void nuevoObjetivo(uint8_t lecturaActual)
{
	uint8_t intentos = 0;

	// Se busca un objetivo separado
	// del punto donde esta el pot.
	do
	{
		objetivo = (uint8_t)(
		20U + aleatorioHasta(61)
		);

		intentos++;
	}
	while (
	abs(
	(int)objetivo -
	(int)lecturaActual
	) < POT_MIN_DISTANCE &&
	intentos < 100U
	);

	posicionInicial = lecturaActual;
	extremoAlcanzado = lecturaActual;

	// Determinar sentido obligatorio
	if (objetivo > posicionInicial)
	{
		direccion = 1;
	}
	else
	{
		direccion = -1;
	}

	estabilidadIniciada = 0;
	inicioEstabilidad = 0;

	ultimaPantalla = 0;
}

// ========================================
// MOSTRAR CERRADURA
// ========================================

static void mostrarPot(
uint32_t ahora,
uint8_t lectura)
{
	char fila1[21];
	char fila2[21];
	char fila3[21];
	char fila4[21];

	snprintf(
	fila1,
	sizeof(fila1),
	"CERRADURA - %u/3",
	(unsigned)(medicion + 1U)
	);

	snprintf(
	fila2,
	sizeof(fila2),
	"OBJ: %u%%  %s",
	(unsigned)objetivo,
	direccion > 0 ? "SUBIR" : "BAJAR"
	);

	snprintf(
	fila3,
	sizeof(fila3),
	"LECTURA: %u%%",
	(unsigned)lectura
	);

	if (estabilidadIniciada)
	{
		uint8_t segundos =
		(uint8_t)(
		(ahora - inicioEstabilidad)
		/ 1000UL
		);

		if (segundos > 2U)
		{
			segundos = 2U;
		}

		snprintf(
		fila4,
		sizeof(fila4),
		"ESTABLE: %u/2 SEG",
		(unsigned)segundos
		);
	}
	else
	{
		strcpy(
		fila4,
		"GIRA SIN RETROCEDER"
		);
	}

	mostrar(
	fila1,
	fila2,
	fila3,
	fila4
	);
}

// ========================================
// REINICIAR CERRADURA POR ERROR
// ========================================

static void fallarPot(uint32_t ahora)
{
	if (tiempoRestante > ERROR_PENALTY)
	{
		tiempoRestante -= ERROR_PENALTY;
	}
	else
	{
		tiempoRestante = 0;
	}

	MAX7219_ShowSeconds(tiempoRestante);

	BUZZER_Beep(3900, 300);

	if (tiempoRestante == 0)
	{
		perder(1);
		return;
	}

	// Reiniciar solo la cerradura
	medicion = 0;

	estabilidadIniciada = 0;

	// Crear nueva combinacion desde
	// la posicion actual del potenciometro
	nuevoObjetivo(ADC_GetPercent());

	falloPotHasta = ahora + 1200UL;

	mostrar(
	"ERROR DE CERRADURA",
	"GIRO INCORRECTO",
	"PENALIZACION -15 S",
	"NUEVA COMBINACION"
	);
}

// ========================================
// MOSTRAR CABLES
// ========================================

static void mostrarOrdenCables(void)
{
	char orden[21];
	char siguiente[21];
	char progreso[21];

	snprintf(
	orden,
	sizeof(orden),
	"ORDEN: %u-%u-%u-%u",
	(unsigned)ordenCables[0],
	(unsigned)ordenCables[1],
	(unsigned)ordenCables[2],
	(unsigned)ordenCables[3]
	);

	snprintf(
	siguiente,
	sizeof(siguiente),
	"DESCONECTA CABLE %u",
	(unsigned)ordenCables[posicionCable]
	);

	snprintf(
	progreso,
	sizeof(progreso),
	"PROGRESO: %u/4",
	(unsigned)posicionCable
	);

	mostrar(
	"ETAPA 3 - CABLES",
	orden,
	siguiente,
	progreso
	);
}

// ========================================
// COMENZAR PARTIDA
// ========================================

static void nuevaPartida(uint32_t ahora)
{
	TIMER_SetAccelerated(0);

	semilla ^= ahora;
	semilla ^= (
	(uint32_t)ADC_GetPercent() << 24
	);

	aleatorio();

	prepararRetos();
	prepararOrdenCables();

	posicionPIN = 0;
	posicionCable = 0;
	medicion = 0;

	estabilidadIniciada = 0;
	errorHasta = 0;
	falloPotHasta = 0;

	tiempoRestante = START_SECONDS;

	ultimoSegundo = TIMER_GameMillis();

	parpadeo = 0;
	pantallaEncendida = 1;

	LCD_SetBacklight(1);
	MAX7219_SetEnabled(1);

	// Detiene animacion y muestra 05.00
	MAX7219_ShowSeconds(tiempoRestante);

	BUZZER_Stop();
	CABLES_ClearEvents();

	cambiarEstado(CODE);
	mostrarReto();
}

// ========================================
// INICIALIZAR JUEGO
// ========================================

void JUEGO_Init(void)
{
	TIMER_SetAccelerated(0);

	tiempoRestante = START_SECONDS;

	posicionPIN = 0;
	posicionCable = 0;
	medicion = 0;

	errorHasta = 0;
	falloPotHasta = 0;

	parpadeo = 0;
	pantallaEncendida = 1;

	ultimoSegundo = TIMER_GameMillis();

	cambiarEstado(WAIT_SLEEP);

	// Arranque minimo: la LCD pide pulsar 0.
	mostrarReposo();
}

// ========================================
// ACTUALIZAR CRONOMETRO
// ========================================

static void actualizarTiempo(void)
{
	if (estado != CODE &&
	estado != STABILIZE &&
	estado != CUT_CABLES)
	{
		return;
	}

	uint32_t ahora = TIMER_GameMillis();

	while (tiempoRestante > 0 &&
	(uint32_t)(ahora - ultimoSegundo) >= 1000UL)
	{
		ultimoSegundo += 1000UL;
		tiempoRestante--;

		MAX7219_ShowSeconds(tiempoRestante);

		// Avisar cada segundo del contador al llegar a 30 s.
		// En las etapas aceleradas, sonara cada 0.5 s reales.
		if (tiempoRestante > 0 && tiempoRestante <= 30U)
		{
			if (tiempoRestante <= 10U)
			{
				BUZZER_Beep(1800, 130);
			}
			else
			{
				BUZZER_Beep(1100, 75);
			}
		}
	}

	if (tiempoRestante == 0)
	{
		perder(1);
	}
}

// ========================================
// EFECTO DE FALLO
// ========================================

static void actualizarParpadeo(uint32_t ahora)
{
	if (!parpadeo)
	{
		return;
	}

	if (
	(uint32_t)(
	ahora - ultimoParpadeo
	) < 280UL
	)
	{
		return;
	}

	ultimoParpadeo = ahora;

	pantallaEncendida =
	(uint8_t)!pantallaEncendida;

	MAX7219_SetEnabled(
	pantallaEncendida
	);

	LCD_SetBacklight(
	pantallaEncendida
	);

	if (pantallaEncendida)
	{
		BUZZER_Beep(3900, 100);
	}
}

// ========================================
// ACTUALIZAR JUEGO
// ========================================

void JUEGO_Update(void)
{
	uint32_t ahora = TIMER_Millis();
	uint8_t lecturaPot = ADC_GetPercent();
	char tecla = KEYPAD_GetKey();

	// Actualizar mensaje animado
	MAX7219_Task(ahora);

	actualizarTiempo();

	if (estado == LOSE)
	{
		actualizarParpadeo(ahora);
	}

	switch (estado)
	{
		// --------------------------------
		// REPOSO HASTA PULSAR 0
		// --------------------------------

		case WAIT_SLEEP:
		if (tecla == '0')
		{
			abrirMenu();
		}
		break;

		// --------------------------------
		// ESPERAR INICIO
		// --------------------------------

		case WAIT_START:

		if (tecla == '#')
		{
			// Los cuatro plugs deben estar insertados.
			if (CABLES_AllConnected())
			{
				nuevaPartida(ahora);
			}
			else
			{
				mostrarCablesFaltantes();
				BUZZER_Beep(3900, 200);
			}
		}

		break;

		// --------------------------------
		// ETAPA 1: PIN 
		// --------------------------------

		case CODE:

		if (errorHasta != 0)
		{
			if (
			(int32_t)(
			ahora - errorHasta
			) >= 0
			)
			{
				errorHasta = 0;
				mostrarReto();
			}

			break;
		}

		if (
		tecla >= '0' &&
		tecla <= '9'
		)
		{
			if (
			tecla ==
			respuestas[posicionPIN]
			)
			{
				posicionPIN++;

				BUZZER_Beep(3900, 55);

				if (posicionPIN >= 4)
				{
					// Comienza cerradura
					medicion = 0;
					falloPotHasta = 0;

					nuevoObjetivo(lecturaPot);

					TIMER_SetAccelerated(1);

					ultimoSegundo =
					TIMER_GameMillis();

					cambiarEstado(STABILIZE);

					mostrarPot(
					ahora,
					lecturaPot
					);
				}
				else
				{
					mostrarReto();
				}
			}
			else
			{
				if (
				tiempoRestante >
				ERROR_PENALTY
				)
				{
					tiempoRestante -=
					ERROR_PENALTY;
				}
				else
				{
					tiempoRestante = 0;
				}

				
				MAX7219_ShowSeconds(
				tiempoRestante
				);

				// Reiniciar el progreso del PIN
				posicionPIN = 0;

				// Generar cuatro ejercicios nuevos
				prepararRetos();

				// Sonido de respuesta incorrecta
				BUZZER_Beep(3900, 300);


				if (tiempoRestante == 0)
				{
					perder(1);
				}
				else
				{
					mostrar(
					"CODIGO INCORRECTO",
					"PENALIZACION -15 S",
					"PIN REINICIADO",
					"INTENTA DE NUEVO"
					);

					errorHasta =
					ahora + 1100UL;
				}
			}
		}

		break;

		// --------------------------------
		// ETAPA 2: CERRADURA
		// --------------------------------

		case STABILIZE:
		{
			// Esperar mensaje de penalizacion
			if (falloPotHasta != 0)
			{
				if (
				(int32_t)(
				ahora - falloPotHasta
				) < 0
				)
				{
					break;
				}

				falloPotHasta = 0;

				// Tomar la posicion actual
				// como nuevo punto de partida
				nuevoObjetivo(lecturaPot);
				mostrarPot(ahora, lecturaPot);
			}

			uint8_t errorGiro = 0;

			// --------------------------------
			// GIRO EN DIRECCION ASCENDENTE
			// --------------------------------

			if (direccion > 0)
			{
				// Registrar maximo alcanzado
				if (
				lecturaPot >
				extremoAlcanzado
				)
				{
					extremoAlcanzado = lecturaPot;
				}

				// Se paso del limite superior
				if (
				(int)lecturaPot >
				(int)objetivo +
				POT_TOLERANCE
				)
				{
					errorGiro = 1;
				}

				// Retrocedio demasiado
				if (
				(int)extremoAlcanzado -
				(int)lecturaPot >
				POT_REVERSE_MARGIN
				)
				{
					errorGiro = 1;
				}
			}

			// --------------------------------
			// GIRO EN DIRECCION DESCENDENTE
			// --------------------------------

			else
			{
				// Registrar minimo alcanzado
				if (
				lecturaPot <
				extremoAlcanzado
				)
				{
					extremoAlcanzado = lecturaPot;
				}

				// Se paso del limite inferior
				if (
				(int)lecturaPot <
				(int)objetivo -
				POT_TOLERANCE
				)
				{
					errorGiro = 1;
				}

				// Retrocedio demasiado
				if (
				(int)lecturaPot -
				(int)extremoAlcanzado >
				POT_REVERSE_MARGIN
				)
				{
					errorGiro = 1;
				}
			}

			// --------------------------------
			// ERROR DE CERRADURA
			// --------------------------------

			if (errorGiro)
			{
				fallarPot(ahora);
				break;
			}

			// --------------------------------
			// OBJETIVO DENTRO DE TOLERANCIA
			// --------------------------------

			if (
			abs(
			(int)lecturaPot -
			(int)objetivo
			) <= POT_TOLERANCE
			)
			{
				if (!estabilidadIniciada)
				{
					estabilidadIniciada = 1;
					inicioEstabilidad = ahora;
				}
			}
			else
			{
				estabilidadIniciada = 0;
			}

			// --------------------------------
			// DOS SEGUNDOS REALES ESTABLES
			// --------------------------------

			if (
			estabilidadIniciada &&
			(uint32_t)(
			ahora - inicioEstabilidad
			) >= POT_STABLE_MS
			)
			{
				medicion++;

				BUZZER_Beep(3900, 140);

				if (medicion >= 3)
				{
					TIMER_SetAccelerated(0);

					cambiarEstado(WAIT_CABLES);

					CABLES_ClearEvents();

					mostrar(
					"CERRADURA ABIERTA",
					"3/3 COMBINACIONES",
					"CONECTA LOS 4 CABLES",
					"PARA CONTINUAR"
					);
				}
				else
				{
					// Generar siguiente numero
					// desde la posicion actual
					nuevoObjetivo(lecturaPot);

					mostrarPot(
					ahora,
					lecturaPot
					);
				}
			}

			else if (
			(uint32_t)(
			ahora - ultimaPantalla
			) >= 700UL
			)
			{
				ultimaPantalla = ahora;

				mostrarPot(
				ahora,
				lecturaPot
				);
			}

			break;
		}

		// --------------------------------
		// ESPERAR CABLES CONECTADOS
		// --------------------------------

		case WAIT_CABLES:


		if (CABLES_AllConnected())
		{
			CABLES_ClearEvents();

			posicionCable = 0;

			tiempoRestante = WIRE_SECONDS;

			MAX7219_ShowSeconds(
			tiempoRestante
			);

			TIMER_SetAccelerated(1);

			ultimoSegundo =
			TIMER_GameMillis();

			cambiarEstado(CUT_CABLES);

			mostrarOrdenCables();
		}

		break;

		// --------------------------------
		// ETAPA 3: CABLES
		// --------------------------------

		case CUT_CABLES:
		{
			uint8_t desconectado;

			while (
			(desconectado =
			CABLES_NextDisconnected()) != 0
			)
			{
				if (
				desconectado !=
				ordenCables[posicionCable]
				)
				{
					perder(0);
					break;
				}

				posicionCable++;

				BUZZER_Beep(3900, 90);

				if (posicionCable >= 4)
				{
					ganar();
					break;
				}

				mostrarOrdenCables();
			}

			break;
		}

		// --------------------------------
		// VICTORIA Y DERROTA
		// --------------------------------

		case WIN:
		case LOSE:

		if (tecla == '#')
		{
			parpadeo = 0;
			pantallaEncendida = 1;

			BUZZER_Stop();

			LCD_SetBacklight(1);
			MAX7219_SetEnabled(1);

			TIMER_SetAccelerated(0);

			tiempoRestante = START_SECONDS;

			cambiarEstado(WAIT_START);

			mostrarInicio();

			MAX7219_StartScroll("HOLA JUEGA");
			BUZZER_PlayWelcome();
		}

		break;

		default:
		break;
	}
}
