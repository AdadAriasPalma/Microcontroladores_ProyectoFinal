# Simulador de Lógica y Reacción en Tiempo Real Basado en ATmega328P

Prototipo electrónico interactivo de desactivación desarrollado en lenguaje C (*bare-metal*) para el microcontrolador **ATmega328P** a 16 MHz. El sistema implementa una arquitectura cooperativa no bloqueante gobernada por interrupciones de hardware, integrando múltiples periféricos sin el uso de un sistema operativo de tiempo real (RTOS) ni retardos bloqueantes (`_delay_ms`).

**Repositorio oficial:** [github.com/AdadAriasPalma/Microcontroladores_ProyectoFinal](https://github.com/AdadAriasPalma/Microcontroladores_ProyectoFinal)

---

## Arquitectura y Características Técnicas

* **Microcontrolador:** ATmega328P @ 16 MHz (compilado en Microchip Studio).
* **Arquitectura de Firmware:** Bucle principal cooperativo basado en máquinas de estados finitos (`juego.c`) y tareas periódicas no bloqueantes (`ADC_Task`, `KEYPAD_Task`, `CABLES_Task`, `BUZZER_Task`, `LCD_Task`, `MAX7219_Task`).
* **Interrupciones de Hardware:**
  * `TIMER1_COMPA_vect`: Base de tiempo determinista de `1.000 ms` (Modo CTC, prescaler 64, `OCR1A = 249`) con doble contador atómico (`tiempoReal` y `tiempoJuego` con aceleración $2\times$ por software).
  * `INT0_vect`: Detección por flanco ascendente en `PD2` para la desconexión crítica del cuarto cable, protegida mediante bloques atómicos (`ATOMIC_BLOCK`).
* **Periféricos Integrados:**
  * **TIMER1:** Temporización dual en tiempo real y tiempo acelerado (`timer.c`).
  * **I2C / TWI:** Bus a 100 kHz con protección por *timeout* hacia expansor PCF8574 (`0x27`) para pantalla **LCD 20×4** (`i2c.c`, `lcd.c`).
  * **SPI (Software):** Comunicación serial síncrona de 16 bits hacia controlador **MAX7219** de 8 dígitos de 7 segmentos (`max7219.c`).
  * **ADC:** Conversión de 10 bits en `ADC0` (`PC0`) a 125 kHz con muestreo cada 10 ms e inversión de escala para potenciómetro lineal de $10\text{ k}\Omega$ (`adc.c`).
  * **PWM:** Timer 2 en modo *Phase Correct PWM* (`OC2B` / `PD3`) para generación de tonos y melodías almacenadas en memoria Flash `PROGMEM` (`buzzer.c`).
  * **Teclado Matricial:** Teclado físico cableado como matriz 4×3 con escaneo cada 10 ms y antirrebote de 35 ms (`keypad.c`).

---

## Asignación de Pines (Pinout ATmega328P)

| Componente / Módulo | Señal | Pin Arduino UNO | Pin ATmega328P | Configuración |
| :--- | :--- | :---: | :---: | :--- |
| **Buzzer Pasivo** | Señal PWM | D3 | `PD3` (`OC2B`) | Salida PWM (Reposo `HIGH`) |
| **Cable Desmontable 1** | Detección | D4 | `PD4` | Entrada con *Pull-up* interno |
| **Cable Desmontable 2** | Detección | D5 | `PD5` | Entrada con *Pull-up* interno |
| **Cable Desmontable 3** | Detección | D6 | `PD6` | Entrada con *Pull-up* interno |
| **Cable Desmontable 4** | Interrupción | D2 | `PD2` (`INT0`) | Entrada con *Pull-up* + Flanco subida |
| **Teclado Matricial** | Columna 1 | D7 | `PD7` | Entrada con *Pull-up* interno |
| **Teclado Matricial** | Columna 2 | D8 | `PB0` | Entrada con *Pull-up* interno |
| **Teclado Matricial** | Columna 3 | D9 | `PB1` | Entrada con *Pull-up* interno |
| **Teclado Matricial** | Fila 1 | D12 | `PB4` | Salida digital |
| **Teclado Matricial** | Fila 2 | A1 | `PC1` | Salida digital |
| **Teclado Matricial** | Fila 3 | A2 | `PC2` | Salida digital |
| **Teclado Matricial** | Fila 4 | A3 | `PC3` | Salida digital |
| **Display MAX7219** | `CS` / `LOAD` | D10 | `PB2` | Salida digital |
| **Display MAX7219** | `DIN` | D11 | `PB3` | Salida digital |
| **Display MAX7219** | `CLK` | D13 | `PB5` | Salida digital |
| **Potenciómetro 10kΩ** | Cursor | A0 | `PC0` (`ADC0`) | Entrada analógica |
| **LCD 20×4 (PCF8574)** | `SDA` | A4 | `PC4` (`SDA`) | Bus TWI / I2C (`0x27`) |
| **LCD 20×4 (PCF8574)** | `SCL` | A5 | `PC5` (`SCL`) | Bus TWI / I2C (`0x27`) |

---

## Flujo de Operación (Máquina de Estados)

1. **Modo de Reposo (`WAIT_SLEEP`):** Sistema silencioso con cronómetro apagado a la espera de pulsar `0`.
2. **Menú Principal (`WAIT_START`):** Melodía electrónica de bienvenida, verificación de que los 4 conectores jack de 2.5 mm estén insertados e inicio de partida con `#` (tiempo límite inicial: `05:00`).
3. **Etapa 1 — Código Dinámico (`CODE`):** Resolución de 4 retos generados pseudoaleatoriamente (aritmética modular, álgebra lineal, conteo/paridad binaria y compuertas lógicas). Un error penaliza `-15 s`, borra el progreso y regenera los cuatro ejercicios.
4. **Etapa 2 — Cerradura Analógica (`STABILIZE`):** Ajuste consecutivo de 3 porcentajes objetivo mediante el potenciómetro con tolerancia de $\pm 3\%$, margen máximo de retroceso de $6\%$ y ventana de estabilidad de `2.0 s` reales bajo reloj acelerado.
5. **Etapa 3 — Secuencia de Desconexión (`WAIT_CABLES` $\rightarrow$ `CUT_CABLES`):** Desconexión ordenada de los 4 cables en un límite de 30 segundos acelerados. Cualquier desconexión fuera de secuencia dispara el fallo crítico inmediato (`LOSE`).

---

## Estructura del Repositorio

```text
Microcontroladores_ProyectoFinal/
├── src/            # Código fuente modular en C (.c y .h) para Microchip Studio
├── proteus/        # Esquema de simulación (.pdsprj) y binario compilado (.hex)
├── figuras/        # Evidencias gráficas (esquemático, simulación y prototipo físico)
├── reporte/        # Código fuente LaTeX (IEEEtran) y PDF del reporte técnico
└── README.md
