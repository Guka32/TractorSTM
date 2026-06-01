/**
 ******************************************************************************
 * @file           : task_timing.h
 * @brief          : Task timing measurement and characterization
 * @description    : Measure WCET using TIM4, generate task tables and timelines
 * 
 * STM32F103RB Configuration:
 * - Clock: 64 MHz
 * - TIM4 Prescaler: 999 (1 kHz clock = 1 ms per 1000 ticks)
 * - Resolution: 15.625 µs per tick
 * 
 ******************************************************************************
 */

#ifndef INC_TASK_TIMING_H_
#define INC_TASK_TIMING_H_

#include <stdint.h>

/* ============================================================================
   TASK DEFINITIONS
   ============================================================================ */

typedef enum {
    TASK_READ_THROTTLE = 0,
    TASK_READ_BRAKE,
    TASK_ENGINE_CONTROL,
    TASK_UPDATE_PWM,
    TASK_SEND_TELEMETRY,
    TASK_UPDATE_LCD,
    TASK_COUNT
} TaskID_t;

typedef struct {
    const char *name;
    uint32_t period_ms;              /* Task period in milliseconds */
    uint32_t deadline_ms;            /* Deadline in milliseconds */
    uint8_t priority_rms;            /* RMS priority (1=highest) */
    uint32_t wcet_us;                /* Worst-case execution time (microseconds) */
    uint32_t bcet_us;                /* Best-case execution time (microseconds) */
    uint32_t avg_us;                 /* Average execution time */
} TaskCharacteristics_t;

/* ============================================================================
   TIMING MEASUREMENT FUNCTIONS
   ============================================================================ */

/**
 * Initialize TIM4 for timing measurements
 * PSC = 999 → 15.625 µs resolution at 64 MHz
 */
void TASK_TIMING_Init(void);

/**
 * Reset and start timing measurement
 * Call this BEFORE the code you want to measure
 */
void TASK_TIMING_Start(void);

/**
 * Stop timing and return elapsed ticks
 * @return: Elapsed ticks from last TASK_TIMING_Start()
 */
uint16_t TASK_TIMING_Stop(void);

/**
 * Convert timer ticks to microseconds
 * @param ticks: Timer ticks from TIM4->CNT
 * @return: Time in microseconds (float)
 */
float TASK_TIMING_TicksToUs(uint16_t ticks);

/**
 * Record timing sample for a task
 * Updates WCET, BCET, and average
 * 
 * @param task_id: Task identifier
 * @param ticks: Measured execution time in ticks
 */
void TASK_TIMING_RecordSample(TaskID_t task_id, uint16_t ticks);

/**
 * Print task characteristics table in tabular format
 * Output format suitable for Serial Wire Viewer
 */
void TASK_TIMING_PrintCharacteristics(void);

/**
 * Print scheduling timeline (200 ms hyperperiod)
 * Shows task execution pattern over one complete cycle
 */
void TASK_TIMING_PrintTimeline(void);

/**
 * Print summary statistics for all tasks
 */
void TASK_TIMING_PrintSummary(void);

/**
 * Reset all timing measurements
 */
void TASK_TIMING_ResetAll(void);

/**
 * Get task characteristics
 * @param task_id: Task identifier
 * @return: Pointer to TaskCharacteristics_t structure
 */
const TaskCharacteristics_t* TASK_TIMING_GetCharacteristics(TaskID_t task_id);

/**
 * Get number of samples collected for a task
 * @param task_id: Task identifier
 * @return: Number of timing samples
 */
uint32_t TASK_TIMING_GetSampleCount(TaskID_t task_id);

#endif /* INC_TASK_TIMING_H_ */
