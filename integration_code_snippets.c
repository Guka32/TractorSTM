/**
 ******************************************************************************
 * @file           : integration_code_snippets.c
 * @brief          : Code snippets to integrate into your main_uart.c
 * 
 * COPY AND PASTE these snippets into your main_uart.c
 * 
 ******************************************************************************
 */

/* ============================================================================
   STEP 1: ADD INCLUDE AT THE TOP OF main_uart.c
   ============================================================================ */

// Add this line near other #includes:
#include "task_timing.h"


/* ============================================================================
   STEP 2: MODIFY main() FUNCTION - INITIALIZATION PART
   ============================================================================ */

// In main(), after other initializations, add:

int main(void)
{
    /* Loop forever */
    USER_SystemClock_Config( );
    USER_GPIO_Init( );
    USER_GPIO_LED_Init( );
    USER_USART2_Init( );
    USER_ADC1_Init( );
    USER_PWM4_Init( );
    USER_TIM2_Init40ms( );
    LCD_Init( );

    /* Initialize motor control */
    USER_Motor_Init( );
    EngTrModel_initialize( );

    /* ========== ADD THESE LINES ========== */
    TASK_TIMING_Init();              // Initialize TIM4 for timing
    TASK_TIMING_ResetAll();          // Reset all measurements
    
    static uint32_t cycle_count = 0;
    static uint32_t measurement_cycles = 100;  // Measure 100 cycles
    /* ====================================== */

    __asm volatile ("cpsie i");

    USER_USART2_SendString("====================================\r\n");
    USER_USART2_SendString("TRANSMISSION CONTROL SYSTEM - STM32F103RB\r\n");
    USER_USART2_SendString("====================================\r\n");
    USER_USART2_SendString("System initialized successfully!\r\n");
    /* ========== ADD THIS LINE ========== */
    USER_USART2_SendString("Collecting timing data...\r\n");
    /* ==================================== */
    USER_USART2_SendString("Ready for 40 ms control loop.\r\n");
    USER_USART2_SendString("====================================\r\n\r\n");

    /* ... rest of main() ... */
}


/* ============================================================================
   STEP 3: MODIFY MAIN LOOP - TASK 1: READ THROTTLE
   ============================================================================ */

// In the main for(;;) loop, replace:
/*
    uint16_t throttleRaw = USER_ADC1_ReadThrottleRaw();
    uint8_t targetThrottle = USER_ClampPercentFromRaw(throttleRaw);
*/

// With:
for(;;){
    if (USER_TIM2_ConsumeTick() != 0U) {
        static uint8_t lcdTickDivider = 0U;
        static real_T currentThrottle = 0.0;
        static real_T currentBrake = 0.0;
        
        /* ========== TASK 1: READ THROTTLE ========== */
        TASK_TIMING_Start();
        uint16_t throttleRaw = USER_ADC1_ReadThrottleRaw();
        uint8_t targetThrottle = USER_ClampPercentFromRaw(throttleRaw);
        uint16_t ticks_t1 = TASK_TIMING_Stop();
        
        if (cycle_count < measurement_cycles) {
            TASK_TIMING_RecordSample(TASK_READ_THROTTLE, ticks_t1);
        }
        /* ============================================ */
        
        // ... rest of code ...
    }
}


/* ============================================================================
   STEP 4: MODIFY MAIN LOOP - TASK 2: READ BRAKE
   ============================================================================ */

// Replace:
/*
    uint8_t brakeActive = USER_ReadBrakeState( );
*/

// With:
/* ========== TASK 2: READ BRAKE ========== */
TASK_TIMING_Start();
uint8_t brakeActive = USER_ReadBrakeState();
uint16_t ticks_t2 = TASK_TIMING_Stop();

if (cycle_count < measurement_cycles) {
    TASK_TIMING_RecordSample(TASK_READ_BRAKE, ticks_t2);
}
/* ========================================= */


/* ============================================================================
   STEP 5: MODIFY MAIN LOOP - TASK 3: ENGINE CONTROL
   ============================================================================ */

// Replace:
/*
    EngTrModel_U.Throttle = currentThrottle;
    EngTrModel_U.BrakeTorque = currentBrake;
    EngTrModel_step( );
*/

// With:
/* ========== TASK 3: ENGINE CONTROL ========== */
TASK_TIMING_Start();
EngTrModel_U.Throttle = currentThrottle;
EngTrModel_U.BrakeTorque = currentBrake;
EngTrModel_step();
uint16_t ticks_t3 = TASK_TIMING_Stop();

if (cycle_count < measurement_cycles) {
    TASK_TIMING_RecordSample(TASK_ENGINE_CONTROL, ticks_t3);
}
/* ============================================ */


/* ============================================================================
   STEP 6: MODIFY MAIN LOOP - TASK 4: UPDATE PWM
   ============================================================================ */

// Replace:
/*
    uint8_t ledDuty = throttlePercent;
    USER_PWM4_SetDutyPercent(ledDuty);
*/

// With:
/* ========== TASK 4: UPDATE PWM ========== */
TASK_TIMING_Start();
uint8_t ledDuty = throttlePercent;
USER_PWM4_SetDutyPercent(ledDuty);
uint16_t ticks_t4 = TASK_TIMING_Stop();

if (cycle_count < measurement_cycles) {
    TASK_TIMING_RecordSample(TASK_UPDATE_PWM, ticks_t4);
}
/* ======================================== */


/* ============================================================================
   STEP 7: MODIFY MAIN LOOP - TASK 5: SEND TELEMETRY
   ============================================================================ */

// Replace:
/*
    USER_USART2_SendTelemetry(throttlePercent, brakeActive, engineRpm, vehicleSpeed, gear);
*/

// With:
/* ========== TASK 5: SEND TELEMETRY ========== */
TASK_TIMING_Start();
USER_USART2_SendTelemetry(throttlePercent, brakeActive, engineRpm, vehicleSpeed, gear);
uint16_t ticks_t5 = TASK_TIMING_Stop();

if (cycle_count < measurement_cycles) {
    TASK_TIMING_RecordSample(TASK_SEND_TELEMETRY, ticks_t5);
}
/* ============================================ */


/* ============================================================================
   STEP 8: MODIFY MAIN LOOP - TASK 6: UPDATE LCD
   ============================================================================ */

// Replace:
/*
    lcdTickDivider++;
    if (lcdTickDivider >= 5U) {
        lcdTickDivider = 0U;
        USER_LCD_UpdateStatus(engineRpm, vehicleSpeed, gear);
    }
*/

// With:
/* ========== TASK 6: UPDATE LCD ========== */
lcdTickDivider++;
if (lcdTickDivider >= 5U) {
    lcdTickDivider = 0U;
    
    TASK_TIMING_Start();
    USER_LCD_UpdateStatus(engineRpm, vehicleSpeed, gear);
    uint16_t ticks_t6 = TASK_TIMING_Stop();
    
    if (cycle_count < measurement_cycles) {
        TASK_TIMING_RecordSample(TASK_UPDATE_LCD, ticks_t6);
    }
}
/* ========================================= */


/* ============================================================================
   STEP 9: ADD MEASUREMENT CONTROL AT END OF LOOP
   ============================================================================ */

// After all tasks, at the end of the if (USER_TIM2_ConsumeTick() != 0U) block:

        cycle_count++;
        
        /* Print results after collecting samples */
        if (cycle_count == measurement_cycles) {
            USER_USART2_SendString("\r\n\r\n");
            USER_USART2_SendString("================================\r\n");
            USER_USART2_SendString("MEASUREMENT COMPLETE!\r\n");
            USER_USART2_SendString("================================\r\n\r\n");
            
            TASK_TIMING_PrintCharacteristics();
            TASK_TIMING_PrintTimeline();
            TASK_TIMING_PrintSummary();
            
            USER_USART2_SendString("\r\n");
            USER_USART2_SendString("Data collection finished.\r\n");
        }
    }  // End of if (USER_TIM2_ConsumeTick() != 0U)
}  // End of for(;;)


/* ============================================================================
   COMPLETE MODIFIED main() STRUCTURE
   ============================================================================ */

/*
int main(void)
{
    // === SYSTEM INITIALIZATION ===
    USER_SystemClock_Config();
    USER_GPIO_Init();
    USER_GPIO_LED_Init();
    USER_USART2_Init();
    USER_ADC1_Init();
    USER_PWM4_Init();
    USER_TIM2_Init40ms();
    LCD_Init();
    
    USER_Motor_Init();
    EngTrModel_initialize();
    
    // === TIMING MEASUREMENT INITIALIZATION ===
    TASK_TIMING_Init();
    TASK_TIMING_ResetAll();
    
    static uint32_t cycle_count = 0;
    static uint32_t measurement_cycles = 100;
    
    __asm volatile ("cpsie i");
    
    USER_USART2_SendString("System initialized...\r\n");
    USER_USART2_SendString("Collecting timing data...\r\n");
    
    // === MAIN CONTROL LOOP ===
    for(;;) {
        if (USER_TIM2_ConsumeTick() != 0U) {
            static uint8_t lcdTickDivider = 0U;
            static real_T currentThrottle = 0.0;
            static real_T currentBrake = 0.0;
            
            // TASK 1: READ THROTTLE
            TASK_TIMING_Start();
            uint16_t throttleRaw = USER_ADC1_ReadThrottleRaw();
            uint8_t targetThrottle = USER_ClampPercentFromRaw(throttleRaw);
            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_READ_THROTTLE, TASK_TIMING_Stop());
            }
            
            // TASK 2: READ BRAKE
            TASK_TIMING_Start();
            uint8_t brakeActive = USER_ReadBrakeState();
            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_READ_BRAKE, TASK_TIMING_Stop());
            }
            
            // Brake logic...
            if (brakeActive != 0U) {
                currentThrottle -= 1.0;
                if (currentThrottle < 0.0) currentThrottle = 0.0;
                currentBrake += 2.0;
                if (currentBrake > 100.0) currentBrake = 100.0;
            } else {
                currentThrottle = (real_T)targetThrottle;
                currentBrake = 0.0;
            }
            
            uint8_t throttlePercent = (uint8_t)currentThrottle;
            
            // Jumpstart logic...
            if (EngTrModel_Y.EngineSpeed < 50.0 && currentThrottle > 2.0) {
                EngTrModel_initialize();
            }
            
            // TASK 3: ENGINE CONTROL
            TASK_TIMING_Start();
            EngTrModel_U.Throttle = currentThrottle;
            EngTrModel_U.BrakeTorque = currentBrake;
            EngTrModel_step();
            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_ENGINE_CONTROL, TASK_TIMING_Stop());
            }
            
            USER_Debug_ModelEngineSpeed = EngTrModel_Y.EngineSpeed;
            USER_Debug_ModelVehicleSpeed = EngTrModel_Y.VehicleSpeed;
            USER_Debug_ModelGear = EngTrModel_Y.Gear;
            
            uint16_t engineRpm = USER_ClampRpm(EngTrModel_Y.EngineSpeed);
            uint16_t vehicleSpeed = USER_ClampSpeed(EngTrModel_Y.VehicleSpeed);
            uint8_t gear = (EngTrModel_Y.Gear <= 0.0) ? 0U : 
                          (uint8_t)(EngTrModel_Y.Gear + 0.5);
            
            USER_Debug_ClampedEngineRpm = engineRpm;
            USER_Debug_ClampedVehicleSpeed = vehicleSpeed;
            USER_Debug_ClampedGear = gear;
            
            // TASK 4: UPDATE PWM
            TASK_TIMING_Start();
            uint8_t ledDuty = throttlePercent;
            USER_PWM4_SetDutyPercent(ledDuty);
            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_UPDATE_PWM, TASK_TIMING_Stop());
            }
            
            // TASK 5: SEND TELEMETRY
            TASK_TIMING_Start();
            USER_USART2_SendTelemetry(throttlePercent, brakeActive, engineRpm, vehicleSpeed, gear);
            if (cycle_count < measurement_cycles) {
                TASK_TIMING_RecordSample(TASK_SEND_TELEMETRY, TASK_TIMING_Stop());
            }
            
            // TASK 6: UPDATE LCD
            lcdTickDivider++;
            if (lcdTickDivider >= 5U) {
                lcdTickDivider = 0U;
                
                TASK_TIMING_Start();
                USER_LCD_UpdateStatus(engineRpm, vehicleSpeed, gear);
                if (cycle_count < measurement_cycles) {
                    TASK_TIMING_RecordSample(TASK_UPDATE_LCD, TASK_TIMING_Stop());
                }
            }
            
            // MEASUREMENT CONTROL
            cycle_count++;
            if (cycle_count == measurement_cycles) {
                TASK_TIMING_PrintCharacteristics();
                TASK_TIMING_PrintTimeline();
                TASK_TIMING_PrintSummary();
            }
        }
    }
    
    return 0;
}
*/

