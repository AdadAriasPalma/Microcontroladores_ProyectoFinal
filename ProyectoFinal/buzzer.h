
#ifndef BUZZER_H_
#define BUZZER_H_

#include <stdint.h>

void BUZZER_Init(void);

void BUZZER_Beep(
uint16_t frequency_hz,
uint16_t duration_ms
);

void BUZZER_Stop(void);

// Melodia repetitiva de bienvenida
void BUZZER_PlayWelcome(void);

// Melodia navidena de victoria
void BUZZER_PlayVictory(void);

// Actualizar sonidos sin bloquear el juego
void BUZZER_Task(void);

#endif
