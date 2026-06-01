# CHECKLIST COMPLETO: MEDICIÓN DE TIEMPOS Y ANÁLISIS DE TAREAS

## 📋 ARCHIVOS CREADOS

| Archivo | Descripción | Ubicación |
|:--------|:-----------|:----------|
| `task_timing.h` | Header con definiciones de tareas y funciones | `Inc/` |
| `task_timing.c` | Implementación de medición de tiempos | `Src/` |
| `main_timing_integration_example.c` | Ejemplo completo de integración | Raíz del proyecto |
| `integration_code_snippets.c` | Snippets de código para copiar/pegar | Raíz del proyecto |
| `TIMING_MEASUREMENT_GUIDE.md` | Guía paso a paso (este documento) | Raíz del proyecto |

---

## 🚀 QUICKSTART (5 MINUTOS)

### Para los apurados:

```bash
1. Copia task_timing.h a Inc/
2. Copia task_timing.c a Src/
3. Abre main_timing_integration_example.c
4. Reemplaza main() en tu main_uart.c con el del ejemplo
5. Compilar y ejecutar
6. Captura salida en Serial Wire Viewer
```

---

## 📝 CHECKLIST DETALLADO

### FASE 1: PREPARACIÓN

- [ ] Verificar que `task_timing.h` está en `Inc/`
- [ ] Verificar que `task_timing.c` está en `Src/`
- [ ] Verificar que proyecto compila sin errores
- [ ] Verificar conexión USB del NUCLEO-F103RB
- [ ] Verificar Serial Wire Viewer configurado (puerto COM, 9600 baud)

### FASE 2: INTEGRACIÓN

Opción A (Rápida - Recomendada):
- [ ] Abrir `main_timing_integration_example.c`
- [ ] Copiar función `main()` completa
- [ ] Reemplazar `main()` en `main_uart.c`
- [ ] Compilar

Opción B (Manual - Paso a paso):
- [ ] Agregar `#include "task_timing.h"` en `main_uart.c`
- [ ] Llamar a `TASK_TIMING_Init()` después de inicialización
- [ ] Llamar a `TASK_TIMING_ResetAll()` al inicio
- [ ] Envolver T1 (ReadThrottle) con `TASK_TIMING_Start/Stop`
- [ ] Envolver T2 (ReadBrake) con `TASK_TIMING_Start/Stop`
- [ ] Envolver T3 (EngineControl) con `TASK_TIMING_Start/Stop`
- [ ] Envolver T4 (UpdatePWM) con `TASK_TIMING_Start/Stop`
- [ ] Envolver T5 (SendTelemetry) con `TASK_TIMING_Start/Stop`
- [ ] Envolver T6 (UpdateLCD) con `TASK_TIMING_Start/Stop`
- [ ] Agregar contador `cycle_count` y `measurement_cycles`
- [ ] Compilar

### FASE 3: EJECUCIÓN

- [ ] Compilar el proyecto (Ctrl+B o Project → Build)
- [ ] Sin errores de compilación
- [ ] Cargar en STM32 (Ctrl+F11 o Run → Run)
- [ ] Abrir Serial Wire Viewer
- [ ] Conectar a puerto COM correcto
- [ ] Ver inicialización del sistema
- [ ] Observar recolección de datos (100 ciclos)

### FASE 4: CAPTURA DE DATOS

- [ ] Esperar a que se completen las 100 mediciones
- [ ] Ver salida de tablas en Serial Wire Viewer:
  - [ ] `TASK CHARACTERISTICS TABLE`
  - [ ] `SCHEDULING TIMELINE`
  - [ ] `TIMING MEASUREMENT SUMMARY`
- [ ] Seleccionar toda la salida (Ctrl+A)
- [ ] Copiar (Ctrl+C)
- [ ] Guardar en archivo `timing_results.txt`

### FASE 5: ANÁLISIS

- [ ] Revisar tabla de características
- [ ] Verificar que todas las tareas tienen WCET > 0
- [ ] Revisar valores WCET:
  - [ ] T1 (ReadThrottle): 5-50 µs
  - [ ] T2 (ReadBrake): 5-20 µs
  - [ ] T3 (EngineControl): 1000-2500 µs
  - [ ] T4 (UpdatePWM): 20-100 µs
  - [ ] T5 (SendTelemetry): 500-2000 µs
  - [ ] T6 (UpdateLCD): 2000-6000 µs
- [ ] Calcular utilización CPU: U = Σ(WCET_i / T_i) / 1000
- [ ] Verificar U < 100% (planificable)
- [ ] Revisar timeline de calendarización
- [ ] Verificar que no hay overlaps

### FASE 6: DOCUMENTACIÓN

- [ ] Crear archivo `TASK_ANALYSIS_RESULTS.md`
- [ ] Copiar tabla de características
- [ ] Copiar timeline
- [ ] Agregar análisis de utilización CPU
- [ ] Agregar conclusiones
- [ ] Guardar en raíz del proyecto

---

## 🔍 TABLA DE TAREAS ESPERADA

```
╔════╦════════════════╦═════════════╦═════════════╦═════════════╦════════════╦══════════════════╗
║ ID ║ Task Name      ║ Period (ms) ║ Deadline    ║ WCET (µs)   ║ BCET (µs)  ║ RMS Priority     ║
╠════╬════════════════╬═════════════╬═════════════╬═════════════╬════════════╬══════════════════╣
║ T1 ║ ReadThrottle   ║          40 ║          40 ║        [?]  ║        [?] ║                5 ║
║ T2 ║ ReadBrake      ║          40 ║          40 ║        [?]  ║        [?] ║                2 ║
║ T3 ║ EngineControl  ║          40 ║          40 ║        [?]  ║        [?] ║                1 ║
║ T4 ║ UpdatePWM      ║          40 ║          40 ║        [?]  ║        [?] ║                3 ║
║ T5 ║ SendTelemetry  ║          40 ║          40 ║        [?]  ║        [?] ║                4 ║
║ T6 ║ UpdateLCD      ║         200 ║         200 ║        [?]  ║        [?] ║                2 ║
╚════╩════════════════╩═════════════╩═════════════╩═════════════╩════════════╩══════════════════╝

[?] = Valores que medirás
```

---

## 📊 TIMELINE ESPERADA

```
Time (ms):  0     20      40      60      80     100    120     140    160    180    200
            |------|------|------|------|------|------|------|------|------|------|------|
T1 R...     [#]             [#]             [#]             [#]             [#]
T2 Re...    [#]             [#]             [#]             [#]             [#]
T3 En...    [####]          [####]          [####]          [####]          [####]
T4 Up...    [##]            [##]            [##]            [##]            [##]
T5 Se...    [###]           [###]           [###]           [###]           [###]
T6 Up...    [#####]                                                         [#####]
```

---

## 🔧 SOLUCIÓN DE PROBLEMAS

### ❌ No compila

```
Error: 'task_timing.h' file not found
```

**Solución:**
1. Verifica que `task_timing.h` está en `Inc/`
2. Click derecho en proyecto → Refresh
3. Reconstruir: Project → Clean → Build

---

### ❌ No se ven datos en Serial Wire Viewer

**Solución:**
1. Verifica puerto COM en Serial Wire Viewer
2. Verifica baudrate (9600)
3. Reconecta el NUCLEO
4. Verifica que USART2 está configurado en `user_uart.c`

---

### ❌ WCET = 0 para todas las tareas

**Solución:**
1. Verifica que `TASK_TIMING_Init()` se llamó
2. Verifica que `TASK_TIMING_RecordSample()` se llama para cada tarea
3. Verifica que `cycle_count < measurement_cycles` es verdadero
4. Aumenta `measurement_cycles` a 200 si es necesario

---

### ❌ Tiempos sospechosamente grandes

**Posible causa:**
- TIM4 no inicializó correctamente
- PSC incorrecto (debe ser 999)
- Wrapping de tareas mide código adicional

**Solución:**
1. Verifica `TASK_TIMING_Init()` - PSC debe ser 999
2. Verifica que `TASK_TIMING_Stop()` está al final de la tarea

---

## 📚 ESTRUCTURA DEL PROYECTO FINAL

```
Tu_Proyecto/
├── Inc/
│   ├── main.h
│   ├── user_timer.h
│   ├── user_adc.h
│   ├── user_uart.h
│   ├── task_timing.h          ← NUEVO
│   └── ... otros headers
├── Src/
│   ├── main_uart.c             ← MODIFICADO
│   ├── user_timer.c
│   ├── user_adc.c
│   ├── user_uart.c
│   ├── task_timing.c           ← NUEVO
│   └── ... otros archivos
├── Debug/
├── main_timing_integration_example.c  ← REFERENCIA
├── integration_code_snippets.c        ← SNIPPETS
├── TIMING_MEASUREMENT_GUIDE.md        ← GUÍA
├── timing_results.txt                 ← SALIDA (generar)
└── TASK_ANALYSIS_RESULTS.md          ← ANÁLISIS (generar)
```

---

## 📈 PARÁMETROS DE CONFIGURACIÓN

### Clock Configuration
- **Oscillator**: HSI (Internal)
- **Frequency**: 64 MHz
- **PLL**: Habilitado (x16)

### TIM4 Configuration (Medición)
- **Clock**: APB1 (64 MHz)
- **Prescaler (PSC)**: 999
- **Resolution**: 15.625 µs/tick
- **Range**: 15.625 µs a 1.024 s

### TIM2 Configuration (Control Loop)
- **Clock**: APB1 (64 MHz)
- **Period**: 40 ms
- **Interrupt**: Habilitado

### USART2 Configuration (Comunicación)
- **Baudrate**: 9600 bps
- **Data bits**: 8
- **Stop bits**: 1
- **Parity**: None

---

## ✅ VERIFICACIÓN FINAL

Antes de pasar a la siguiente fase, verifica:

- [ ] Código compila sin errores
- [ ] Código compila sin warnings
- [ ] Se ejecuta en el STM32F103RB
- [ ] Serial Wire Viewer muestra datos
- [ ] Tabla de características se imprime
- [ ] Timeline se visualiza
- [ ] Resumen de utilización se calcula
- [ ] Todos los valores WCET > 0
- [ ] Utilización CPU calculada correctamente

---

## 🎯 PRÓXIMOS PASOS

Después de completar este análisis:

1. **PARTE II: Migración a FreeRTOS** (si aplica)
   - Convertir tareas a funciones FreeRTOS
   - Asignar prioridades según RMS
   - Crear semáforos para sincronización

2. **Documentación del proyecto**
   - Crear memoria del análisis
   - Documentar decisiones de diseño
   - Guardar historial de mediciones

3. **Optimización (si es necesario)**
   - Si U > 100%: Aumentar períodos o reducir WCET
   - Si alguna tarea no es planificable: Revisar deadline

---

## 💡 CONSEJOS ÚTILES

1. **Medir múltiples veces**: Los tiempos pueden variar según el estado del sistema
2. **Buscar patrones**: Si ves un patrón periódico → Buscar causa
3. **Documentar todo**: Guarda los resultados para futuras optimizaciones
4. **Validar WCET**: El WCET debe ser realista (no demasiado grande)
5. **Considerar interferencia**: Otros periféricos pueden afectar tiempos

---

## 📞 SOPORTE

Si encuentras problemas:

1. Verifica que los archivos están en los directorios correctos
2. Revisa que todos los `#include` están presentes
3. Verifica configuración de puerto COM
4. Reconstruye el proyecto (Clean → Build)
5. Reconecta el NUCLEO

---

¡Éxito con tu análisis de tareas! 🚀

