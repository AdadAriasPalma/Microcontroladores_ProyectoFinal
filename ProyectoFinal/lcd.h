
#ifndef LCD_H_
#define LCD_H_

#include <stdint.h>

// Direccion I2C de la pantalla LCD
#define LCD_I2C_ADDRESS 0x27

// Dimensiones de la pantalla
#define LCD_COLS 20
#define LCD_ROWS 4

// Inicializar la pantalla
void LCD_Init(void);

// Actualizar la pantalla periodicamente
void LCD_Task(void);

// Comprobar si la pantalla esta lista
uint8_t LCD_IsReady(void);

// Mostrar texto en dos filas
void LCD_SetText(
const char *line1,
const char *line2
);

// Mostrar texto en las cuatro filas
void LCD_SetText4(
const char *line1,
const char *line2,
const char *line3,
const char *line4
);

// Controlar la retroiluminacion
// 1 = encendida, 0 = apagada
void LCD_SetBacklight(uint8_t enabled);

#endif
