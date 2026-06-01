/**
 ******************************************************************************
 * @file           : task_timing.c
 * @brief          : Task timing measurement and characterization implementation
 * 
 ******************************************************************************
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "task_timing.h"

/* ============================================================================
   CONSTANTS
   ============================================================================ */

#define TIM4_PRESCALER          999U
#define SYSCLK_MHZ              64U
#define TICKS_PER_US            (SYSCLK_MHZ / (TIM4_PRESCALER + 1))  /* 0.064 */

/* ============================================================================
   TASK CHARACTERIZATION
   ============================================================================ */

/* Initial task definitions based on system requirements */
static TaskCharacteristics_t tasks[TASK_COUNT] = {
    /* T1: ReadThrottle */
    {
        .name = "ReadThrottle",
        .period_ms = 40,
        .deadline_ms = 40,
        .priority_rms = 5,
        .wcet_us = 0,
        .bcet_us = 0xFFFFFFFF,
        .avg_us = 0
    },
    /* T2: ReadBrake */
    {
        .name = "ReadBrake",
        .period_ms = 40,
        .deadline_ms = 40,
        .priority_rms = 2,
        .wcet_us = 0,
        .bcet_us = 0xFFFFFFFF,
        .avg_us = 0
    },
    /* T3: EngineControl */
    {
        .name = "EngineControl",
        .period_ms = 40,
        .deadline_ms = 40,
        .priority_rms = 1,
        .wcet_us = 0,
        .bcet_us = 0xFFFFFFFF,
        .avg_us = 0
    },
    /* T4: UpdatePWM */
    {
        .name = "UpdatePWM",
        .period_ms = 40,
        .deadline_ms = 40,
        .priority_rms = 3,
        .wcet_us = 0,
        .bcet_us = 0xFFFFFFFF,
        .avg_us = 0
    },
    /* T5: SendTelemetry */
    {
        .name = "SendTelemetry",
        .period_ms = 40,
        .deadline_ms = 40,
        .priority_rms = 4,
        .wcet_us = 0,
        .bcet_us = 0xFFFFFFFF,
        .avg_us = 0
    },
    /* T6: UpdateLCD */
    {
        .name = "UpdateLCD",
        .period_ms = 200,
        .deadline_ms = 200,
        .priority_rms = 2,
        .wcet_us = 0,
        .bcet_us = 0xFFFFFFFF,
        .avg_us = 0
    }
};

/* Sample counting */
static uint32_t sample_count[TASK_COUNT] = {0};
static uint32_t total_samples[TASK_COUNT] = {0};

/* ============================================================================
   INITIALIZATION
   ============================================================================ */

void TASK_TIMING_Init(void)
{
    /* Enable TIM4 clock (APB1) */
    RCC->APB1ENR |= (0x1UL << 2U);

    /* Configure TIM4 */
    TIM4->CR1 &= ~(0x3UL << 5U)      /* CMS bits (edge-aligned) */
             & ~(0x1UL << 4U)        /* DIR bit (upcounter) */
             & ~(0x1UL << 1U);       /* UDIS bit (UEV enabled) */

    /* Internal clock */
    TIM4->SMCR &= ~(0x7UL << 0U);

    /* Set prescaler */
    TIM4->PSC = TIM4_PRESCALER;

    /* Auto-reload register (max range) */
    TIM4->ARR = 0xFFFFU;

    /* Clear counter */
    TIM4->CNT = 0U;

    /* Clear status register */
    TIM4->SR = 0U;

    /* Generate update event */
    TIM4->EGR |= (0x1UL << 0U);

    /* Enable counter */
    TIM4->CR1 |= (0x1UL << 0U);
}

/* ============================================================================
   TIMING MEASUREMENT
   ============================================================================ */

void TASK_TIMING_Start(void)
{
    TIM4->CNT = 0U;
}

uint16_t TASK_TIMING_Stop(void)
{
    return (uint16_t)(TIM4->CNT);
}

float TASK_TIMING_TicksToUs(uint16_t ticks)
{
    /* Time = Ticks * (1000 / 64MHz) µs
       = Ticks * 15.625 µs */
    return (float)ticks * (1000.0f / 64000000.0f);
}

void TASK_TIMING_RecordSample(TaskID_t task_id, uint16_t ticks)
{
    if (task_id >= TASK_COUNT) {
        return;
    }

    float time_us = TASK_TIMING_TicksToUs(ticks);

    /* Update WCET (maximum) */
    if (time_us > (float)tasks[task_id].wcet_us) {
        tasks[task_id].wcet_us = (uint32_t)time_us;
    }

    /* Update BCET (minimum) */
    if (time_us < (float)tasks[task_id].bcet_us) {
        tasks[task_id].bcet_us = (uint32_t)time_us;
    }

    /* Update average */
    if (sample_count[task_id] == 0) {
        tasks[task_id].avg_us = (uint32_t)time_us;
    } else {
        tasks[task_id].avg_us = (tasks[task_id].avg_us * sample_count[task_id] + (uint32_t)time_us)
                              / (sample_count[task_id] + 1);
    }

    sample_count[task_id]++;
    total_samples[task_id]++;
}

/* ============================================================================
   DISPLAY FUNCTIONS
   ============================================================================ */

void TASK_TIMING_PrintCharacteristics(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                       TASK CHARACTERISTICS TABLE - STM32F103RB                                  ║\n");
    printf("╠════╦════════════════╦═════════════╦═════════════╦═════════════╦════════════╦══════════════════╣\n");
    printf("║ ID ║ Task Name      ║ Period (ms) ║ Deadline    ║ WCET (µs)   ║ BCET (µs)  ║ RMS Priority     ║\n");
    printf("╠════╬════════════════╬═════════════╬═════════════╬═════════════╬════════════╬══════════════════╣\n");

    for (int i = 0; i < TASK_COUNT; i++) {
        char wcet_str[12] = "---";
        char bcet_str[12] = "---";

        if (tasks[i].wcet_us > 0) {
            snprintf(wcet_str, sizeof(wcet_str), "%lu", tasks[i].wcet_us);
        }
        if (tasks[i].bcet_us < 0xFFFFFFFF) {
            snprintf(bcet_str, sizeof(bcet_str), "%lu", tasks[i].bcet_us);
        }

        printf("║ T%d ║ %-14s ║ %11lu ║ %11lu ║ %11s ║ %10s ║ %16d ║\n",
               i + 1,
               tasks[i].name,
               tasks[i].period_ms,
               tasks[i].deadline_ms,
               wcet_str,
               bcet_str,
               tasks[i].priority_rms);
    }

    printf("╚════╩════════════════╩═════════════╩═════════════╩═════════════╩════════════╩══════════════════╝\n");
}

void TASK_TIMING_PrintTimeline(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    SCHEDULING TIMELINE (200 ms Hyperperiod)                                    ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    /* Timeline scale: 200 ms hyperperiod */
    printf("Time (ms):  0     20      40      60      80     100     120     140     160     180     200\n");
    printf("            |------|------|------|------|------|------|------|------|------|------|------|---\n");

    /* Draw each task */
    for (int i = 0; i < TASK_COUNT; i++) {
        printf("T%d %-13s", i + 1, tasks[i].name);

        uint32_t period = tasks[i].period_ms;
        uint32_t wcet_ticks = (uint32_t)((tasks[i].wcet_us / 1000.0f) * 2);  /* Scale for display */

        /* Draw task executions */
        for (uint32_t t = 0; t < 200; t += period) {
            printf("[");
            for (uint32_t j = 0; j < wcet_ticks && j < 5; j++) {
                printf("█");
            }
            printf("]");

            /* Spacing to next execution */
            uint32_t spacing = (period / 10) - wcet_ticks - 1;
            for (uint32_t j = 0; j < spacing; j++) {
                printf(" ");
            }
        }
        printf("\n");
    }

    printf("\n");
    printf("Legend:  [████] = Task execution | Tick = 1 ms\n");
    printf("\n");
}

void TASK_TIMING_PrintSummary(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                          TIMING MEASUREMENT SUMMARY                                           ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════════════════════╣\n");

    uint32_t total_wcet = 0;
    uint32_t hyperperiod_lcm = 200;  /* LCM(40, 40, 40, 40, 40, 200) = 200 */

    for (int i = 0; i < TASK_COUNT; i++) {
        if (sample_count[i] > 0) {
            printf("│ %-88s │\n", " ");
            printf("│ Task T%d: %s                                                                             │\n",
                   i + 1, tasks[i].name);
            printf("│   - Samples collected:    %lu                                                              │\n",
                   sample_count[i]);
            printf("│   - WCET:                 %lu µs                                                              │\n",
                   tasks[i].wcet_us);
            printf("│   - BCET:                 %lu µs                                                              │\n",
                   tasks[i].bcet_us);
            printf("│   - Average:              %lu µs                                                              │\n",
                   tasks[i].avg_us);
            printf("│   - Utilization (U):      %.2f %%                                                             │\n",
                   (float)tasks[i].wcet_us / (float)tasks[i].period_ms * 1000.0f);

            total_wcet += tasks[i].wcet_us;
        }
    }

    printf("│ %-88s │\n", " ");
    printf("│ CPU Utilization Analysis:                                                                     │\n");
    printf("│   - Total U:               %.2f %%                                                             │\n",
           (float)total_wcet / (float)hyperperiod_lcm * 100.0f);
    printf("│   - Hyperperiod:           %lu ms                                                              │\n",
           hyperperiod_lcm);

    printf("╚════════════════════════════════════════════════════════════════════════════════════════════════╝\n");
}

/* ============================================================================
   UTILITY FUNCTIONS
   ============================================================================ */

void TASK_TIMING_ResetAll(void)
{
    memset(sample_count, 0, sizeof(sample_count));
    
    for (int i = 0; i < TASK_COUNT; i++) {
        tasks[i].wcet_us = 0;
        tasks[i].bcet_us = 0xFFFFFFFF;
        tasks[i].avg_us = 0;
    }
}

const TaskCharacteristics_t* TASK_TIMING_GetCharacteristics(TaskID_t task_id)
{
    if (task_id >= TASK_COUNT) {
        return NULL;
    }
    return &tasks[task_id];
}

uint32_t TASK_TIMING_GetSampleCount(TaskID_t task_id)
{
    if (task_id >= TASK_COUNT) {
        return 0;
    }
    return sample_count[task_id];
}
