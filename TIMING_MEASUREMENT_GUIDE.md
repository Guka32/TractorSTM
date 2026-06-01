# GUÍA PASO A PASO: MEDICIÓN DE TIEMPOS Y ANÁLISIS DE TAREAS
## STM32F103RB - Tractor Control System

---

## RESUMEN GENERAL

Tu tarea es:
1. **Definir características de tareas** (períodos, prioridades, WCET)
2. **Medir tiempos de ejecución reales** usando TIM4
3. **Generar tabla de tareas** con características
4. **Generar timeline de calendarización**
5. **Visualizar en Serial Wire Viewer**

Se proporcionan 3 archivos:
- `task_timing.h` - Declaraciones de funciones
- `task_timing.c` - Implementación de medición
- `main_timing_integration_example.c` - Ejemplo completo

---

## PASO 1: ENTENDER TU SISTEMA ACTUAL

### Tareas Identificadas

| ID | Tarea | Período | Descripción |
|:--:|:------|:-------:|:-----------|
| T1 | ReadThrottle | 40 ms | Lee ADC potenciómetro (PA0) |
| T2 | ReadBrake | 40 ms | Lee botón freno (PC13) |
| T3 | EngineControl | 40 ms | Ejecuta modelo Simulink |
| T4 | UpdatePWM | 40 ms | Actualiza LEDs PWM |
| T5 | SendTelemetry | 40 ms | Transmite por UART |
| T6 | UpdateLCD | 200 ms | Actualiza pantalla (cada 5 × 40ms) |

### Configuración Hardware

- **Clock**: STM32F103RB @ 64 MHz
- **TIM2**: Tick base cada 40 ms (control loop)
- **TIM4**: Para medición de tiempos (PSC=999 → 15.625 µs resolución)
- **Tick mínimo**: 1 ms

---

## PASO 2: INTEGRAR CÓDIGO DE MEDICIÓN

### 2.1 Copiar archivos al proyecto

```
Tu proyecto:
├── Inc/
│   ├── main.h
│   ├── user_timer.h
│   └── task_timing.h          ← NUEVO
├── Src/
│   ├── main_uart.c
│   ├── user_timer.c
│   └── task_timing.c          ← NUEVO
└── main_timing_integration_example.c  ← REFERENCIA
```

**Acciones:**
1. Copia `task_timing.h` → Tu carpeta `Inc/`
2. Copia `task_timing.c` → Tu carpeta `Src/`
3. En STM32CubeIDE: Click derecho en proyecto → Refresh

### 2.2 Actualizar include en tu main_uart.c

```c
#include "task_timing.h"    // AGREGAR ESTA LÍNEA
```

### 2.3 Modificar main() para integrar medición

**Opción A: Rápida (cambios mínimos)**

En tu `main()` actual en `main_uart.c`, después de inicialización:

```c
int main(void) {
    USER_SystemClock_Config();
    // ... resto de inicialización ...
    
    /* AGREGAR ESTAS LÍNEAS */
    TASK_TIMING_Init();              // Inicializar TIM4
    TASK_TIMING_ResetAll();          // Resetear mediciones
    
    /* ... resto del código ... */
    
    for(;;) {
        if (USER_TIM2_ConsumeTick() != 0U) {
            /* WRAPPEAR CADA TAREA */
            
            // T1: ReadThrottle
            TASK_TIMING_Start();
            uint16_t throttleRaw = USER_ADC1_ReadThrottleRaw();
            // ... resto de T1 ...
            TASK_TIMING_RecordSample(TASK_READ_THROTTLE, TASK_TIMING_Stop());
            
            // T2: ReadBrake
            TASK_TIMING_Start();
            uint8_t brakeActive = USER_ReadBrakeState();
            // ... resto de T2 ...
            TASK_TIMING_RecordSample(TASK_READ_BRAKE, TASK_TIMING_Stop());
            
            // ... etc para T3-T6 ...
        }
    }
}
```

**Opción B: Completa (reemplazar main)**

Usa `main_timing_integration_example.c` como referencia completa:
1. Abre `main_timing_integration_example.c`
2. Copia el `main()` completo
3. Pégalo en tu `main_uart.c` reemplazando el actual

---

## PASO 3: INTEGRACIÓN DETALLADA DE CADA TAREA

### Estructura de Wrapping

```c
/* Antes de la tarea */
TASK_TIMING_Start();

/* [CÓDIGO DE LA TAREA AQUÍ] */

/* Después de la tarea */
uint16_t ticks = TASK_TIMING_Stop();
TASK_TIMING_RecordSample(TASK_ID, ticks);
```

### Ejemplo: T1 ReadThrottle

```c
/* TASK 1: READ THROTTLE */
TASK_TIMING_Start();
uint16_t throttleRaw = USER_ADC1_ReadThrottleRaw();
uint8_t targetThrottle = USER_ClampPercentFromRaw(throttleRaw);
uint16_t ticks = TASK_TIMING_Stop();

if (cycle_count < measurement_cycles) {
    TASK_TIMING_RecordSample(TASK_READ_THROTTLE, ticks);
}
```

### Ejemplo: T3 EngineControl (MÁS COMPLEJO)

```c
/* TASK 3: ENGINE CONTROL */
TASK_TIMING_Start();
EngTrModel_U.Throttle = currentThrottle;
EngTrModel_U.BrakeTorque = currentBrake;
EngTrModel_step();         // ← AQUÍ ESTÁ EL PESO
uint16_t ticks = TASK_TIMING_Stop();

if (cycle_count < measurement_cycles) {
    TASK_TIMING_RecordSample(TASK_ENGINE_CONTROL, ticks);
}
```

### Ejemplo: T6 UpdateLCD (CONDICIONAL)

**IMPORTANTE**: Solo mide cuando REALMENTE ejecuta LCD

```c
/* TASK 6: UPDATE LCD (every 200 ms) */
lcdTickDivider++;
if (lcdTickDivider >= 5U) {
    lcdTickDivider = 0U;
    
    TASK_TIMING_Start();
    USER_LCD_UpdateStatus(engineRpm, vehicleSpeed, gear);
    uint16_t ticks = TASK_TIMING_Stop();
    
    if (cycle_count < measurement_cycles) {
        TASK_TIMING_RecordSample(TASK_UPDATE_LCD, ticks);
    }
}
```

---

## PASO 4: COMPILACIÓN Y CARGA

### 4.1 Compilar

```bash
Project → Build Project
# O presiona Ctrl+B
```

**Si hay errores:**
- Verificar que `task_timing.h` está en `Inc/`
- Verificar que `task_timing.c` está en `Src/`
- Verificar que se agregó `#include "task_timing.h"`

### 4.2 Cargar en el STM32

- Conecta el NUCLEO-F103RB por USB
- Run → Run Configurations → Run
- O presiona Ctrl+F11

---

## PASO 5: CAPTURAR DATOS EN SERIAL WIRE VIEWER

### 5.1 Configurar Serial Wire Viewer

1. **En STM32CubeIDE**:
   - Window → Show View → Serial Wire Viewer

2. **Configurar puerto**:
   - Puerto: COM (el de tu NUCLEO)
   - Baudrate: 9600 (o el que uses)
   - Bits: 8, Stop: 1, Parity: None

3. **Conectar**:
   - Click en "Connect"

### 5.2 Ejecutar el programa

El programa recolectará 100 ciclos de mediciones y luego imprimirá:

```
════════════════════════════════════════════════════════════════════
║                   TASK CHARACTERISTICS TABLE                      ║
╠════╦════════════════╦═════════════╦═════════════╦═════════════╦════╣
║ ID ║ Task Name      ║ Period (ms) ║ Deadline    ║ WCET (µs)   ║ ... ║
╠════╬════════════════╬═════════════╬═════════════╬═════════════╬════╣
║ T1 ║ ReadThrottle   ║          40 ║          40 ║        XXX  ║ ... ║
║ T2 ║ ReadBrake      ║          40 ║          40 ║        XXX  ║ ... ║
║ T3 ║ EngineControl  ║          40 ║          40 ║       XXXX  ║ ... ║
...
```

### 5.3 Guardar salida

- Selecciona todo (Ctrl+A) en Serial Wire Viewer
- Copia (Ctrl+C)
- Pega en un archivo `timing_results.txt`

---

## PASO 6: INTERPRETAR RESULTADOS

### Tabla de Características

Se genera una tabla con:
- **ID**: Identificador de tarea (T1-T6)
- **Task Name**: Nombre descriptivo
- **Period (ms)**: Período de ejecución
- **Deadline**: Plazo para finalizar
- **WCET (µs)**: Peor tiempo de ejecución (IMPORTANTE)
- **BCET (µs)**: Mejor tiempo de ejecución
- **RMS Priority**: Prioridad según Rate Monotonic Scheduling

### Timeline de Calendarización

```
Time (ms):  0     20      40      60      80     100    120     140    160
            |------|------|------|------|------|------|------|------|---
T1 ReadThrottle   [█]            [█]            [█]            [█]
T2 ReadBrake      [█]            [█]            [█]            [█]
T3 EngineControl  [████]         [████]         [████]         [████]
T4 UpdatePWM      [██]           [██]           [██]           [██]
T5 SendTelemetry  [███]          [███]          [███]          [███]
T6 UpdateLCD      [██████]                                      [██████]
```

### Resumen de Utilización CPU

```
CPU Utilization Analysis:
   - Total U:     XX.XX %
   - Hyperperiod: 200 ms
```

---

## PASO 7: VALORES ESPERADOS Y ANÁLISIS

### Tiempos Típicos (Estimaciones)

| Tarea | WCET Estimado |
|:------|:----------:|
| T1: ReadThrottle | 10-30 µs |
| T2: ReadBrake | 5-15 µs |
| T3: EngineControl | 1000-2500 µs |
| T4: UpdatePWM | 20-50 µs |
| T5: SendTelemetry | 500-1500 µs |
| T6: UpdateLCD | 2000-5000 µs |

### Cálculo de Utilización CPU

```
U = Σ(WCET_i / T_i)

Ejemplo:
U = (25/40000) + (10/40000) + (1500/40000) + (35/40000) + (1000/40000) + (3500/200000)
U = 0.0625 + 0.0250 + 3.75 + 0.0875 + 2.50 + 1.75
U ≈ 8.1% 

✓ Planificable (< 100%)
```

### Validación RMS

Para n tareas con período corto:
```
U_RM = n × (2^(1/n) - 1) × 100%

Para 5 tareas con período 40ms:
U_RM ≈ 74.5%

Si U < U_RM → ✓ Definitivamente planificable
Si U_RM < U < 100% → Puede ser planificable
Si U > 100% → ✗ No planificable
```

---

## PASO 8: VERIFICACIÓN DE WCET

### Importante: Condiciones de Medición

Para obtener WCET real, asegúrate de:

1. **T1 ReadThrottle**: Medir cuando potenciómetro varía
2. **T2 ReadBrake**: Medir cuando botón presionado/soltado
3. **T3 EngineControl**: Ejecuta COMPLETO (siempre)
4. **T4 UpdatePWM**: Medir cuando PWM cambia
5. **T5 SendTelemetry**: Medir transmisión COMPLETA
6. **T6 UpdateLCD**: **SOLO cuando ejecuta** (cada 200 ms)

### Verificar Mediciones

En Serial Wire Viewer, verifica:
```
Task T3: EngineControl
   - Samples collected:    100
   - WCET:                 1523 µs    ← Confiar en este valor
   - BCET:                 1245 µs
   - Average:              1400 µs
```

Si ves "Samples: 0" → La tarea no se ejecutó (revisar condiciones)

---

## PASO 9: GENERACIÓN DE DOCUMENTACIÓN

Después de obtener los datos, crea un documento:

### `TASK_ANALYSIS_RESULTS.md`

```markdown
# Task Analysis Results - STM32F103RB

## Measured WCET Values

| Task | WCET (µs) | Period (ms) | U (%) |
|:-----|:---------:|:-----------:|:-----:|
| T1   | 25        | 40          | 0.06  |
| T2   | 10        | 40          | 0.03  |
| T3   | 1523      | 40          | 3.81  |
| T4   | 35        | 40          | 0.09  |
| T5   | 1200      | 40          | 3.00  |
| T6   | 3500      | 200         | 1.75  |

**Total Utilization: 8.74%** ✓ Planificable

## Hyperperiod
- LCM(40, 40, 40, 40, 40, 200) = 200 ms
- One complete schedule repeats every 200 ms

## RMS Priorities
[Tabla con prioridades]

## Timeline
[Visualización del timeline]
```

---

## SOLUCIÓN DE PROBLEMAS

### Problema: "No se ven los datos en Serial Wire Viewer"

**Solución:**
1. Verifica que USART2 está habilitado en `user_uart.c`
2. Verifica baudrate (9600 es estándar)
3. Verifica que el cable USB está conectado
4. Intenta reconectar en Serial Wire Viewer

### Problema: "WCET es 0 para todas las tareas"

**Solución:**
1. Verifica que `TASK_TIMING_Init()` fue llamado
2. Verifica que `TASK_TIMING_RecordSample()` es llamado para cada tarea
3. Verifica que `cycle_count < measurement_cycles` es verdadero
4. Aumenta `measurement_cycles` si es necesario

### Problema: "Tiempos demasiado grandes"

**Posibles causas:**
1. TIM4 no inicializó correctamente
2. PSC incorrecto (debe ser 999)
3. Wrapping de tareas incorrecto (mide código extra)

---

## PRÓXIMOS PASOS

Después de completar el análisis:

1. **Documentar resultados** en `TASK_ANALYSIS_RESULTS.md`
2. **Verificar planificabilidad** (U < 100%)
3. **Migrar a FreeRTOS** (si es requerido)
4. **Configurar prioridades** según RMS

---

## REFERENCIAS

- **Clock**: 64 MHz (HSI)
- **TIM4 PSC**: 999 (resolución 15.625 µs)
- **TIM2**: 40 ms base tick
- **Tick mínimo**: 1 ms
- **Baudrate**: 9600 bps

---

¡Éxito con tu análisis! 🚀

