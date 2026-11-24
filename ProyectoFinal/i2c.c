
#include "i2c.h"
#include <avr/io.h>
#include <util/twi.h>

#define TWI_TIMEOUT 30000U

// Esperar a que termine una operacion TWI
static uint8_t wait_twi(void)
{
	uint16_t timeout = TWI_TIMEOUT;

	while (!(TWCR & (1U << TWINT)))
	{
		if (--timeout == 0)
		return 0;
	}

	return 1;
}

// Finalizar comunicacion I2C
static void stop_twi(void)
{
	TWCR = (1U << TWINT) |
	(1U << TWSTO) |
	(1U << TWEN);
}

// Iniciar comunicacion con un dispositivo
static uint8_t start_twi(uint8_t address7, uint8_t read)
{
	TWCR = (1U << TWINT) |
	(1U << TWSTA) |
	(1U << TWEN);

	if (!wait_twi())
	return 0;

	uint8_t status = (uint8_t)(TWSR & 0xF8);

	if (status != TW_START &&
	status != TW_REP_START)
	return 0;

	TWDR = (uint8_t)(
	(address7 << 1) | (read ? 1U : 0U)
	);

	TWCR = (1U << TWINT) |
	(1U << TWEN);

	if (!wait_twi())
	return 0;

	status = (uint8_t)(TWSR & 0xF8);

	return status == (read ?
	TW_MR_SLA_ACK : TW_MT_SLA_ACK);
}

// Inicializar I2C a 100 kHz
void I2C_Init(void)
{
	TWSR = 0;
	TWBR = 72;  // F_CPU = 16 MHz

	TWCR = (1U << TWEN);
}

// Enviar un byte a un dispositivo I2C
uint8_t I2C_WriteByte(uint8_t address7, uint8_t value)
{
	uint8_t ok = 0;

	if (!start_twi(address7, 0))
	goto done;

	TWDR = value;

	TWCR = (1U << TWINT) |
	(1U << TWEN);

	if (!wait_twi())
	goto done;

	ok = ((TWSR & 0xF8) == TW_MT_DATA_ACK);

	done:
	stop_twi();
	return ok;
}

// Leer un byte de un dispositivo I2C
uint8_t I2C_ReadByte(uint8_t address7, uint8_t *value)
{
	uint8_t ok = 0;

	if (!value)
	return 0;

	if (!start_twi(address7, 1))
	goto done;

	// Lectura de un solo byte: enviar NACK
	TWCR = (1U << TWINT) |
	(1U << TWEN);

	if (!wait_twi())
	goto done;

	if ((TWSR & 0xF8) != TW_MR_DATA_NACK)
	goto done;

	*value = TWDR;
	ok = 1;

	done:
	stop_twi();
	return ok;
}
