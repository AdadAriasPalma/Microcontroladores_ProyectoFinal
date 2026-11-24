
#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>

void I2C_Init(void);

uint8_t I2C_WriteByte(
uint8_t address7,
uint8_t value
);

uint8_t I2C_ReadByte(
uint8_t address7,
uint8_t *value
);

#endif
