/**
 ******************************************************************************
 * @file           : main_timing_integration_example.c
 * @brief          : Example of how to integrate task timing into main.c
 * 
 * INSTRUCTIONS:
 * 1. Replace the main() function in your main_uart.c with this code
 * 2. Add #include "task_timing.h" at the top
 * 3. Integrate TASK_TIMING_Start/Stop around each task
 * 4. Run the program and capture Serial Wire Viewer output
 * 
 ******************************************************************************
 */

#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "EngTrModel.h"
#include "lcd.h"
#include "user_adc.h"
#include "user_pwm.h"
#include "user_timer.h"
#include "user_uart.h"
#include "user_motor.h"
#include "task_timing.h"

/* Debug globals for model tracing */
volatile real_T USER_Debug_ModelEngineSpeed = 0.0;
volatile real_T USER_Debug_ModelVehicleSpeed = 0.0;
volatile real_T USER_Debug_ModelGear = 0.0;
volatile uint16_t USER_Debug_ClampedEngineRpm = 0U;
volatile uint16_t USER_Debug_ClampedVehicleSpeed = 0U;
volatile uint8_t USER_Debug_ClampedGear = 0U;

/* ============================================================================
   MAIN FUNCTION WITH TIMING INTEGRATION
   ============================================================================ */

int main(void)
{
    /* ========== System Initialization ========== */
    USER_SystemClock_Config();
    USER_GPIO_Init();
    USER_GPIO_LED_Init();
    USER_USART2_Init();
    USER_ADC1_Init();
    USER_PWM4_Init();
    USER_TIM2_Init40ms();
    LCD_Init();

    /* Initialize motor control */
    USER_Motor_Init();
    EngTrModel_initialize();

    /* Initialize timing measurement */
    TASK_TIMING_Init();
    TASK_TIMING_ResetAll();

    __asm volatile ("cpsie i");

    USER_USART2_SendString("\r\n");
    USER_USART2_SendString("====================================\r\n");
    USER_USART2_SendString("TRANSMISSION CONTROL SYSTEM\r\n");
    USER_USART2_SendString("STM32F103RB - TIMING ANALYSIS\r\n");
    USER_USART2_SendString("====================================\r\n");
    USER_USART2_SendString("System initialized successfully!\r\n");
    USER_USART2_SendString("Collecting timing data...\r\n");
    USER_USART2_SendString("====================================\r\n\r\n");

    /* ========== Main Control Loop ========== */
    static uint32_t cycle_count = 0;
    static uint32_t measurement_cycles = 100;  /* Collect 100 cycles */

    for(;;)
    {
        if (USER_TIM2_ConsumeTick() != 0U)
        {
            static uint8_t lcdTickDivider = 0U;
            static real_T currentThrottle = 0.0;
            static real_T currentBrake = 0.0;
            uint16_t ticks;

            /* ================================================================
               TASK 1: READ THROTTLE (ADC)
               ================================================================ */
            TASK_TIMING_Start();
            uint16_t throttleRaw = USER_ADC1_ReadThrottleRaw();
            uint8_t targetThrottle = USER_ClampPercentFromRaw(throttleRaw);
            ticks = TASK_TIMING_Stop();

            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_READ_THROTTLE, ticks);
            }

            /* ================================================================
               TASK 2: READ BRAKE STATE
               ================================================================ */
            TASK_TIMING_Start();
            uint8_t brakeActive = USER_ReadBrakeState();
            ticks = TASK_TIMING_Stop();

            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_READ_BRAKE, ticks);
            }

            /* Brake button logic */
            if (brakeActive != 0U)
            {
                currentThrottle -= 1.0;
                if (currentThrottle < 0.0) {
                    currentThrottle = 0.0;
                }
                currentBrake += 2.0;
                if (currentBrake > 100.0) {
                    currentBrake = 100.0;
                }
            }
            else
            {
                currentThrottle = (real_T)targetThrottle;
                currentBrake = 0.0;
            }

            uint8_t throttlePercent = (uint8_t)currentThrottle;

            /* Jumpstart logic */
            if (EngTrModel_Y.EngineSpeed < 50.0 && currentThrottle > 2.0)
            {
                EngTrModel_initialize();
            }

            /* ================================================================
               TASK 3: ENGINE CONTROL (SIMULINK MODEL STEP)
               ================================================================ */
            TASK_TIMING_Start();
            EngTrModel_U.Throttle = currentThrottle;
            EngTrModel_U.BrakeTorque = currentBrake;
            EngTrModel_step();
            ticks = TASK_TIMING_Stop();

            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_ENGINE_CONTROL, ticks);
            }

            /* Capture model outputs */
            USER_Debug_ModelEngineSpeed = EngTrModel_Y.EngineSpeed;
            USER_Debug_ModelVehicleSpeed = EngTrModel_Y.VehicleSpeed;
            USER_Debug_ModelGear = EngTrModel_Y.Gear;

            uint16_t engineRpm = USER_ClampRpm(EngTrModel_Y.EngineSpeed);
            uint16_t vehicleSpeed = USER_ClampSpeed(EngTrModel_Y.VehicleSpeed);
            uint8_t gear = (EngTrModel_Y.Gear <= 0.0) ? 0U : (uint8_t)(EngTrModel_Y.Gear + 0.5);

            USER_Debug_ClampedEngineRpm = engineRpm;
            USER_Debug_ClampedVehicleSpeed = vehicleSpeed;
            USER_Debug_ClampedGear = gear;

            /* ================================================================
               TASK 4: UPDATE PWM / LEDS
               ================================================================ */
            TASK_TIMING_Start();
            uint8_t ledDuty = throttlePercent;
            USER_PWM4_SetDutyPercent(ledDuty);
            ticks = TASK_TIMING_Stop();

            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_UPDATE_PWM, ticks);
            }

            /* ================================================================
               TASK 5: SEND TELEMETRY (UART)
               ================================================================ */
            TASK_TIMING_Start();
            USER_USART2_SendTelemetry(throttlePercent, brakeActive, engineRpm, vehicleSpeed, gear);
            ticks = TASK_TIMING_Stop();

            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_SEND_TELEMETRY, ticks);
            }

            /* ================================================================
               TASK 6: UPDATE LCD (every 5 ticks = 200ms)
               ================================================================ */
            lcdTickDivider++;
            if (lcdTickDivider >= 5U)
            {
                lcdTickDivider = 0U;

                TASK_TIMING_Start();
                USER_LCD_UpdateStatus(engineRpm, vehicleSpeed, gear);
                ticks = TASK_TIMING_Stop();

                if (cycle_count < measurement_cycles) {
                    TASK_TIMING_RecordSample(TASK_UPDATE_LCD, ticks);
                }
            }

            /* ================================================================
               MEASUREMENT CONTROL
               ================================================================ */
            cycle_count++;

            /* Print results after collecting enough samples */
            if (cycle_count == measurement_cycles)
            {
                USER_USART2_SendString("\r\n\r\n");
                USER_USART2_SendString("================================\r\n");
                USER_USART2_SendString("MEASUREMENT COMPLETE!\r\n");
                USER_USART2_SendString("================================\r\n\r\n");

                /* Print task characteristics */
                TASK_TIMING_PrintCharacteristics();
                TASK_TIMING_PrintTimeline();
                TASK_TIMING_PrintSummary();

                USER_USART2_SendString("\r\n");
                USER_USART2_SendString("Data collection finished. System continues to run.\r\n");
                USER_USART2_SendString("Cycle count: %lu\r\n", cycle_count);
            }
        }
    }

    return 0;
}

/* ============================================================================
   HELPER FUNCTIONS (Required)
   ============================================================================ */

static uint8_t USER_ReadBrakeState(void)
{
    return ((GPIOC->IDR & (1UL << 13U)) == 0U) ? 1U : 0U;
}

static uint8_t USER_ClampPercentFromRaw(uint16_t rawValue)
{
    if (rawValue > 4095U) {
        rawValue = 4095U;
    }
    return (uint8_t)(((uint32_t)rawValue * 100U) / 4095U);
}

static uint16_t USER_ClampRpm(real_T rpmValue)
{
    if (rpmValue <= 0.0) {
        return 0U;
    }
    if (rpmValue >= 65535.0) {
        return 65535U;
    }
    return (uint16_t)(rpmValue + 0.5);
}

static uint16_t USER_ClampSpeed(real_T speedValue)
{
    if (speedValue <= 0.0) {
        return 0U;
    }
    if (speedValue >= 65535.0) {
        return 65535U;
    }
    return (uint16_t)(speedValue + 0.5);
}

static void USER_LCD_WritePadded(uint8_t line, uint8_t column, const char *text, uint8_t fieldWidth)
{
    char field[17];
    uint8_t i = 0U;

    /* Clear field with spaces */
    for (i = 0U; i < fieldWidth && i < 16U; i++) {
        field[i] = ' ';
    }
    field[i] = '\0';

    /* Copy text to field, right-aligned */
    uint8_t text_len = 0U;
    if (text != NULL) {
        while (text[text_len] != '\0' && text_len < 16U) {
            text_len++;
        }
    }

    if (text_len > fieldWidth) {
        text_len = fieldWidth;
    }

    uint8_t offset = fieldWidth - text_len;
    for (i = 0U; i < text_len && i < 16U; i++) {
        field[offset + i] = text[i];
    }

    LCD_Set_Cursor(line, column);
    LCD_Put_Str(field);
}

static void USER_LCD_UpdateStatus(uint16_t engineRpm, uint16_t vehicleSpeed, uint8_t gear)
{
    char buffer[17];

    /* Line 1: Engine RPM */
    LCD_Set_Cursor(1, 1);
    sprintf(buffer, "RPM:%5u", engineRpm);
    LCD_Put_Str(buffer);

    /* Line 1: Gear */
    LCD_Set_Cursor(1, 11);
    sprintf(buffer, "G:%u", gear);
    LCD_Put_Str(buffer);

    /* Line 2: Vehicle Speed */
    LCD_Set_Cursor(2, 1);
    sprintf(buffer, "SPD:%5u", vehicleSpeed);
    LCD_Put_Str(buffer);
}

void USER_GPIO_Init(void)
{
    RCC->APB2ENR |= (0x1UL << 2U) | (0x1UL << 4U);

    /* PC13 as input pull-up (USER button / Brake) */
    GPIOC->CRH &= ~(0xFUL << 20U);
    GPIOC->CRH |= (0x8UL << 20U);
    GPIOC->ODR |= (0x1UL << 13U);
}

void USER_SystemClock_Config(void)
{
    FLASH->ACR &= ~(0x5UL << 0U);
    FLASH->ACR |= (0x2UL << 0U);
    RCC->CFGR &= ~(0x1UL << 16U) & ~(0x7UL << 11U) & ~(0x3UL << 8U);
    RCC->CFGR |= (0xFUL << 18U) | (0x4UL << 8U);
    RCC->CR |= (0x1UL << 24U);
    while (!(RCC->CR & (0x1UL << 25U)));
    RCC->CFGR &= ~(0x1UL << 0U);
    RCC->CFGR |= (0x2UL << 0U);
    while (0x8UL != (RCC->CFGR & 0xCUL));
}
