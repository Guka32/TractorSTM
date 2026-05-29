/*
 * tasks.c
 * FreeRTOS task implementations for tractor control system
 */

#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tasks.h"
#include "EngTrModel.h"
#include "user_adc.h"
#include "user_pwm.h"
#include "user_uart.h"
#include "lcd.h"

/* ============================================================================
 * Queue and Task Handles (global)
 * ============================================================================ */

QueueHandle_t xThrottleQueue = NULL;
QueueHandle_t xBrakeQueue = NULL;
QueueHandle_t xModelOutputQueue = NULL;
TaskHandle_t xControlTaskHandle = NULL;

/* ============================================================================
 * Static Helper Functions
 * ============================================================================ */

/**
 * @brief Apply smoothing to throttle value
 * Changes by ±1% per 40ms cycle
 */
static real_T USER_SmoothThrottle(real_T currentThrottle, uint8_t targetThrottle)
{
	real_T target = (real_T)targetThrottle;
	
	if (target > currentThrottle) {
		currentThrottle += 1.0;
		if (currentThrottle > target) {
			currentThrottle = target;
		}
	} else if (target < currentThrottle) {
		currentThrottle -= 1.0;
		if (currentThrottle < 0.0) {
			currentThrottle = 0.0;
		}
	}
	
	return currentThrottle;
}

/**
 * @brief Apply smoothing to brake value
 * Increases by +2% per 40ms cycle, resets to 0 when brake released
 */
static real_T USER_SmoothBrake(real_T currentBrake, uint8_t brakeActive)
{
	if (brakeActive != 0U) {
		currentBrake += 2.0;
		if (currentBrake > 100.0) {
			currentBrake = 100.0;
		}
	} else {
		currentBrake = 0.0;
	}
	
	return currentBrake;
}

/**
 * @brief Clamp RPM to valid range (0-8000)
 */
static uint16_t USER_ClampRpm(real_T rpmValue)
{
	if (rpmValue < 0.0) return 0U;
	if (rpmValue > 8000.0) return 8000U;
	return (uint16_t)(rpmValue + 0.5);
}

/**
 * @brief Clamp vehicle speed to valid range (0-100 km/h)
 */
static uint16_t USER_ClampSpeed(real_T speedValue)
{
	if (speedValue < 0.0) return 0U;
	if (speedValue > 100.0) return 100U;
	return (uint16_t)(speedValue + 0.5);
}

/* ============================================================================
 * Task Implementations
 * ============================================================================ */

/**
 * vControlTask - Core control loop task
 * 
 * Triggered by TIM2 ISR every 40ms (highest priority)
 * Runs the Simulink model and coordinates all control actions
 */
void vControlTask(void *pvParameters)
{
	static real_T smoothedThrottle = 0.0;
	static real_T smoothedBrake = 0.0;
	uint8_t throttlePercent = 0U;
	uint8_t brakeActive = 0U;
	ModelOutputs_T outputs;
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* ===== Wait for TIM2 notification (40ms tick) ===== */
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
		/* ===== Receive latest sensor values from queues ===== */
		if (xQueueReceive(xThrottleQueue, &throttlePercent, 0) == pdFALSE) {
			/* If no new throttle data, use 0 */
			throttlePercent = 0U;
		}
		
		if (xQueueReceive(xBrakeQueue, &brakeActive, 0) == pdFALSE) {
			/* If no new brake data, use 0 */
			brakeActive = 0U;
		}
		
		/* ===== Apply smoothing/ramping ===== */
		smoothedThrottle = USER_SmoothThrottle(smoothedThrottle, throttlePercent);
		smoothedBrake = USER_SmoothBrake(smoothedBrake, brakeActive);
		
		/* ===== Jumpstart logic ===== */
		/* Re-initialize model if stalled and throttle applied */
		if (EngTrModel_Y.EngineSpeed < 50.0 && smoothedThrottle > 2.0) {
			EngTrModel_initialize();
		}
		
		/* ===== Set model inputs and execute step ===== */
		EngTrModel_U.Throttle = smoothedThrottle;
		EngTrModel_U.BrakeTorque = smoothedBrake;
		EngTrModel_step();
		
		/* ===== Update LED PWM (responds directly to throttle) ===== */
		USER_PWM4_SetDutyPercent((uint8_t)smoothedThrottle);
		
		/* ===== Prepare output data ===== */
		outputs.engineSpeed = EngTrModel_Y.EngineSpeed;
		outputs.vehicleSpeed = EngTrModel_Y.VehicleSpeed;
		outputs.gear = (EngTrModel_Y.Gear <= 0.0) ? 0U : (uint8_t)(EngTrModel_Y.Gear + 0.5);
		outputs.throttlePercent = (uint8_t)smoothedThrottle;
		outputs.brakeActive = brakeActive;
		
		/* ===== Send outputs to Telemetry task ===== */
		xQueueSend(xModelOutputQueue, &outputs, 0);
	}
}

/**
 * vADCTask - Analog input sampling task
 * 
 * Reads throttle pot and brake button at 40ms rate
 * Sends values to Control task via queue
 */
void vADCTask(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	uint16_t throttleRaw = 0U;
	uint8_t throttlePercent = 0U;
	uint8_t brakeState = 0U;
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* Wait ~40ms, aligned with Control task */
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(40));
		
		/* ===== Read throttle potentiometer (PA0) ===== */
		throttleRaw = USER_ADC1_ReadThrottleRaw();
		
		/* Convert raw ADC (0-4095) to percentage (0-100) */
		throttlePercent = (uint8_t)((throttleRaw * 100U) / 4095U);
		if (throttlePercent > 100U) {
			throttlePercent = 100U;
		}
		
		/* Send to Control task queue (overwrite if not consumed) */
		xQueueSend(xThrottleQueue, &throttlePercent, 0);
		
		/* ===== Read brake button (PC13, active low) ===== */
		/* Assuming USER_ReadBrakeState() returns 1 when brake pressed */
		brakeState = (GPIOC->IDR & (1UL << 13U)) ? 0U : 1U;
		
		/* Send to Control task queue */
		xQueueSend(xBrakeQueue, &brakeState, 0);
	}
}

/**
 * vTelemetryTask - UART telemetry transmission task
 * 
 * Waits for model outputs and sends CSV string to ESP32
 */
void vTelemetryTask(void *pvParameters)
{
	ModelOutputs_T outputs;
	uint16_t engineRpm = 0U;
	uint16_t vehicleSpeed = 0U;
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* Wait for model outputs (with 100ms timeout) */
		if (xQueueReceive(xModelOutputQueue, &outputs, pdMS_TO_TICKS(100)) == pdTRUE) {
			/* Clamp values to valid ranges */
			engineRpm = USER_ClampRpm(outputs.engineSpeed);
			vehicleSpeed = USER_ClampSpeed(outputs.vehicleSpeed);
			
			/* Send telemetry via UART2 */
			USER_USART2_SendTelemetry(
				outputs.throttlePercent,
				outputs.brakeActive,
				engineRpm,
				vehicleSpeed,
				outputs.gear
			);
		}
	}
}

/**
 * vLCDTask - Display update task
 * 
 * Updates HD44780 LCD every 200ms (runs every 40ms but divides by 5)
 * Non-critical task with lowest priority
 */
void vLCDTask(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	uint32_t tickCount = 0U;
	ModelOutputs_T outputs;
	uint16_t engineRpm = 0U;
	uint16_t vehicleSpeed = 0U;
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* 40ms periodic delay */
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(40));
		
		/* ===== Try to get latest model outputs (non-blocking) ===== */
		if (xQueueReceive(xModelOutputQueue, &outputs, 0) == pdTRUE) {
			engineRpm = USER_ClampRpm(outputs.engineSpeed);
			vehicleSpeed = USER_ClampSpeed(outputs.vehicleSpeed);
		}
		
		/* ===== Update LCD every 5th tick (200ms) ===== */
		if ((tickCount % 5U) == 0U) {
			/* Call LCD update function */
			char speedText[8], rpmText[8], gearText[3];
			
			snprintf(speedText, sizeof(speedText), "%3u km/h", vehicleSpeed);
			snprintf(rpmText, sizeof(rpmText), "%4u RPM", engineRpm);
			snprintf(gearText, sizeof(gearText), "G%u", outputs.gear);
			
			/* Write to LCD using correct API: LCD_Set_Cursor + LCD_Put_Str */
			LCD_Set_Cursor(1U, 0U);
			LCD_Put_Str(speedText);
			
			LCD_Set_Cursor(1U, 10U);
			LCD_Put_Str(rpmText);
			
			LCD_Set_Cursor(2U, 0U);
			LCD_Put_Str(gearText);
		}
		
		tickCount++;
	}
}

/**
 * vMotorTask - Motor control task
 * 
 * Placeholder for phase 2 motor driver integration
 * Currently runs at 10ms rate (higher precision for motor control)
 */
void vMotorTask(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* 10ms tick for motor control (will be used for encoder + PID in phase 2) */
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
		
		/* Phase 2: TODO
		 * - Read encoder feedback
		 * - Compute PID outputs
		 * - Update motor PWM
		 */
	}
}

/* ============================================================================
 * Initialization Function
 * ============================================================================ */

/**
 * vCreateAllTasks - Create all FreeRTOS queues and tasks
 * 
 * Called from main() before vTaskStartScheduler()
 */
void vCreateAllTasks(void)
{
	/* ===== Create Queues ===== */
	
	/* Throttle queue: 1 uint8_t item (overwrite if not consumed) */
	xThrottleQueue = xQueueCreate(1, sizeof(uint8_t));
	configASSERT(xThrottleQueue != NULL);
	
	/* Brake queue: 1 uint8_t item */
	xBrakeQueue = xQueueCreate(1, sizeof(uint8_t));
	configASSERT(xBrakeQueue != NULL);
	
	/* Model outputs queue: 1 ModelOutputs_T item */
	xModelOutputQueue = xQueueCreate(1, sizeof(ModelOutputs_T));
	configASSERT(xModelOutputQueue != NULL);
	
	/* ===== Create Tasks ===== */
	
	/* Motor Task - Highest priority (5), will drive motor PWM in phase 2 */
	xTaskCreate(
		vMotorTask,
		"Motor",
		256,
		NULL,
		5,
		NULL
	);
	
	/* Control Task - High priority (4), 40ms model tick */
	xTaskCreate(
		vControlTask,
		"Control",
		512,
		NULL,
		4,
		&xControlTaskHandle
	);
	
	/* ADC Task - Medium priority (3), 40ms sensor read */
	xTaskCreate(
		vADCTask,
		"ADC",
		256,
		NULL,
		3,
		NULL
	);
	
	/* Telemetry Task - Medium-Low priority (2), UART send */
	xTaskCreate(
		vTelemetryTask,
		"Telemetry",
		256,
		NULL,
		2,
		NULL
	);
	
	/* LCD Task - Low priority (1), 200ms display update */
	xTaskCreate(
		vLCDTask,
		"LCD",
		256,
		NULL,
		1,
		NULL
	);
	
	/* ===== Queue Registry (for debugger) ===== */
	vQueueAddToRegistry(xThrottleQueue, "ThrottleQ");
	vQueueAddToRegistry(xBrakeQueue, "BrakeQ");
	vQueueAddToRegistry(xModelOutputQueue, "ModelOutputQ");
}
