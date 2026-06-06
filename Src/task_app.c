/*
 * tasks_app.c
 * FreeRTOS task implementations for tractor control system
 */

#include <stdint.h>
#include <stdio.h>
#include "main.h"
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
QueueHandle_t xTelemetryOutputQueue = NULL;
QueueHandle_t xLCDOutputQueue = NULL;
TaskHandle_t xControlTaskHandle = NULL;
TaskHandle_t xADCTaskHandle = NULL;

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
	(void)currentBrake; /* Parameter kept for compatibility but no longer used for smoothing */
	
	if (brakeActive != 0U) {
		return 100.0; /* Instantly apply 100% brake torque */
	} else {
		return 0.0;   /* Instantly release brake */
	}
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
	static uint8_t throttlePercent = 0U;
	static uint8_t brakeActive = 0U;
	ModelOutputs_T outputs;
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* ===== Wait for TIM2 notification (40ms tick) ===== */
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
		/* ===== Receive latest sensor values from queues ===== */
		xQueueReceive(xThrottleQueue, &throttlePercent, 0);
		xQueueReceive(xBrakeQueue, &brakeActive, 0);
		
		/* ===== Apply smoothing/ramping ===== */
		if (brakeActive != 0U) {
			/* Brake has priority: override throttle to idle (1.45%) */
			smoothedThrottle = 1.45;
			smoothedBrake = USER_SmoothBrake(smoothedBrake, brakeActive);
		} else {
			smoothedThrottle = USER_SmoothThrottle(smoothedThrottle, throttlePercent);
			smoothedBrake = USER_SmoothBrake(smoothedBrake, brakeActive);
		}
		
		/* ===== Prevent Engine Stall ===== */
		/* Force minimum RPM state to 1.0 to prevent the physics model from getting stuck at 0 */
		if (EngTrModel_DW.DiscreteTimeIntegrator_DSTATE < 1.0) {
			EngTrModel_DW.DiscreteTimeIntegrator_DSTATE = 1.0;
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
		
		/* ===== Send outputs to Telemetry and LCD tasks ===== */
		xQueueOverwrite(xTelemetryOutputQueue, &outputs);
		xQueueOverwrite(xLCDOutputQueue, &outputs);
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
	uint16_t lastThrottleRaw = 0U;
	uint8_t throttlePercent = 0U;
	uint8_t brakeState = 0U;
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* 40ms periodic execution */
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(40));
		
		/* Read latest ADC result (polls EOC) */
		throttleRaw = USER_ADC1_ReadThrottleRaw();
		
		/* Hysteresis: only update queue if change > 50 counts (~1.2% of 4095) */
		if ((throttleRaw > (lastThrottleRaw + 50U)) || (throttleRaw < (lastThrottleRaw - 50U))) {
			lastThrottleRaw = throttleRaw;
			
			/* Convert raw ADC (0-4095) to percentage (0-100) */
			throttlePercent = (uint8_t)((throttleRaw * 100U) / 4095U);
			if (throttlePercent > 100U) {
				throttlePercent = 100U;
			}
			
			/* Send to Control task queue (overwrite if not consumed) */
			xQueueOverwrite(xThrottleQueue, &throttlePercent);
		}
		
		/* ===== Read brake button (PC13, active low) every cycle ===== */
		brakeState = (GPIOC->IDR & (1UL << 13U)) ? 0U : 1U;
		xQueueOverwrite(xBrakeQueue, &brakeState);
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
		if (xQueueReceive(xTelemetryOutputQueue, &outputs, pdMS_TO_TICKS(100)) == pdTRUE) {
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
		if (xQueueReceive(xLCDOutputQueue, &outputs, 0) == pdTRUE) {
			engineRpm = USER_ClampRpm(outputs.engineSpeed);
			vehicleSpeed = USER_ClampSpeed(outputs.vehicleSpeed);
		}
		
		/* ===== Update LCD every 5th tick (200ms) ===== */
		if ((tickCount % 5U) == 0U) {
			/* Format display text and pad to clear old characters instead of sending clear command */
			char line1[20], line2[20];
			
			uint16_t dispRpm = (engineRpm > 9999U) ? 9999U : engineRpm;
			uint16_t dispSpeed = (vehicleSpeed > 99U) ? 99U : vehicleSpeed;
			
			snprintf(line1, sizeof(line1), "SPD:%3u RPM:%04u", dispSpeed, dispRpm);
			LCD_Set_Cursor(1U, 1U);
			LCD_Put_Str(line1);
			
			/* Pad with spaces to clear any old characters to the right */
			snprintf(line2, sizeof(line2), "Gear:%-11u", outputs.gear);
			LCD_Set_Cursor(2U, 1U);
			LCD_Put_Str(line2);
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
	
	/* Model outputs queues: 1 ModelOutputs_T item each */
	xTelemetryOutputQueue = xQueueCreate(1, sizeof(ModelOutputs_T));
	configASSERT(xTelemetryOutputQueue != NULL);
	
	xLCDOutputQueue = xQueueCreate(1, sizeof(ModelOutputs_T));
	configASSERT(xLCDOutputQueue != NULL);
	
	/* ===== Create Tasks ===== */
	
	/* Motor Task - Highest priority (4), will drive motor PWM in phase 2 */
	xTaskCreate(
		vMotorTask,
		"Motor",
		128,
		NULL,
		4,
		NULL
	);
	
	/* Control Task - High priority (3), 40ms model tick */
	xTaskCreate(
		vControlTask,
		"Control",
		512,
		NULL,
		3,
		&xControlTaskHandle
	);
	
	/* ADC Task - Medium priority (3), 40ms sensor read */
	xTaskCreate(
		vADCTask,
		"ADC",
		128,
		NULL,
		3,
		&xADCTaskHandle
	);
	
	/* Telemetry Task - Medium-Low priority (2), UART send */
	xTaskCreate(
		vTelemetryTask,
		"Telemetry",
		512,
		NULL,
		2,
		NULL
	);
	
	/* LCD Task - Low priority (1), 200ms display update */
	xTaskCreate(
		vLCDTask,
		"LCD",
		512,
		NULL,
		1,
		NULL
	);
	
	/* ===== Queue Registry (for debugger) ===== */
	vQueueAddToRegistry(xThrottleQueue, "ThrottleQ");
	vQueueAddToRegistry(xBrakeQueue, "BrakeQ");
	vQueueAddToRegistry(xTelemetryOutputQueue, "TelemetryOutQ");
	vQueueAddToRegistry(xLCDOutputQueue, "LCDOutQ");
}
