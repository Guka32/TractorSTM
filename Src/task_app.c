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
#include "user_motor.h"
#include "lcd.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Queue and Task Handles (global)
 * ============================================================================ */

QueueHandle_t xThrottleQueue = NULL;
QueueHandle_t xBrakeQueue = NULL;
QueueHandle_t xTelemetryOutputQueue = NULL;
QueueHandle_t xLCDOutputQueue = NULL;
QueueHandle_t xRemoteQueue = NULL;
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
	static uint8_t localThrottlePercent = 0U;
	static uint8_t localBrakeActive = 0U;
	RemoteCommand_T remoteCmd = {0};
	ModelOutputs_T outputs;
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* ===== Wait for TIM2 notification (40ms tick) ===== */
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
		/* ===== Receive latest sensor values from queues ===== */
		xQueueReceive(xThrottleQueue, &localThrottlePercent, 0);
		xQueueReceive(xBrakeQueue, &localBrakeActive, 0);
		xQueuePeek(xRemoteQueue, &remoteCmd, 0);
		
		uint8_t activeThrottle = localThrottlePercent;
		uint8_t activeBrake = localBrakeActive;
		
		/* If remote command is present and not mode 0, prefer remote */
		if (remoteCmd.mode != 0) {
			activeThrottle = (uint8_t)remoteCmd.throttle;
			activeBrake = (uint8_t)remoteCmd.brake;
		}
		
		/* ===== Apply smoothing/ramping ===== */
		if (activeBrake != 0U) {
			/* Brake has priority: override throttle to idle (1.45%) */
			smoothedThrottle = 1.45;
			smoothedBrake = USER_SmoothBrake(smoothedBrake, activeBrake);
		} else {
			smoothedThrottle = USER_SmoothThrottle(smoothedThrottle, activeThrottle);
			smoothedBrake = USER_SmoothBrake(smoothedBrake, activeBrake);
		}
		
		/* ===== Prevent Engine Stall ===== */
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
		outputs.brakeActive = activeBrake;
		
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
			
			/* Send telemetry via USART3 */
			USER_USART3_SendTelemetry(
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
	RemoteCommand_T currentCmd = {0};
	int16_t rightDelta = 0, leftDelta = 0;
	
	float currentDist = 0.0f;
	float lastTargetDist = 0.0f;
	float Kp = 10.0f; /* Proportional gain for distance */
	float metersPerTick = 0.001f; /* Arbitrary scaling, adjust based on physical wheel size */
	
	(void)pvParameters;  /* Unused parameter */
	
	while (1) {
		/* 10ms tick for motor control (PID + kinematics) */
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
		
		/* Peek at latest remote command */
		xQueuePeek(xRemoteQueue, &currentCmd, 0);
		
		/* Read Encoders */
		USER_Motor_ReadEncoders(&rightDelta, &leftDelta);
		
		if (currentCmd.mode == 0) {
			/* Local Control Mode (Default) */
			uint8_t localBrake = 0;
			uint8_t localThrottle = 0;
			xQueuePeek(xBrakeQueue, &localBrake, 0);
			xQueuePeek(xThrottleQueue, &localThrottle, 0);
			
			USER_Motor_Enable(1);
			if (localBrake) {
				USER_Motor_SetSpeed(MOTOR_FR, 0);
				USER_Motor_SetSpeed(MOTOR_BR, 0);
				USER_Motor_SetSpeed(MOTOR_FL, 0);
				USER_Motor_SetSpeed(MOTOR_BL, 0);
			} else {
				int8_t spd = (int8_t)localThrottle;
				USER_Motor_SetSpeed(MOTOR_FR, spd);
				USER_Motor_SetSpeed(MOTOR_BR, spd);
				USER_Motor_SetSpeed(MOTOR_FL, spd);
				USER_Motor_SetSpeed(MOTOR_BL, spd);
			}
		} else {
			/* Remote Control Mode */
			USER_Motor_Enable(1);
			
			if (currentCmd.brake) {
				USER_Motor_SetSpeed(MOTOR_FR, 0);
				USER_Motor_SetSpeed(MOTOR_BR, 0);
				USER_Motor_SetSpeed(MOTOR_FL, 0);
				USER_Motor_SetSpeed(MOTOR_BL, 0);
			} else if (currentCmd.mode == 1) {
				/* Teleop throttle direct drive */
				int8_t spd = (int8_t)currentCmd.throttle;
				USER_Motor_SetSpeed(MOTOR_FR, spd);
				USER_Motor_SetSpeed(MOTOR_BR, spd);
				USER_Motor_SetSpeed(MOTOR_FL, spd);
				USER_Motor_SetSpeed(MOTOR_BL, spd);
			} else if (currentCmd.mode == 2) {
				/* PID distance control */
				if (currentCmd.distance != lastTargetDist) {
					currentDist = 0.0f;
					lastTargetDist = currentCmd.distance;
				}
				
				float distStep = ((rightDelta + leftDelta) / 2.0f) * metersPerTick;
				currentDist += distStep;
				
				float error = currentCmd.distance - currentDist;
				float pOut = Kp * error;
				
				/* Simple heading hold: if one side moves faster, correct it */
				float headingError = (rightDelta - leftDelta) * metersPerTick;
				float turnComp = headingError * Kp * 2.0f;
				
				int16_t outR = (int16_t)(pOut - turnComp);
				int16_t outL = (int16_t)(pOut + turnComp);
				
				if (outR > 100) outR = 100;
				if (outR < -100) outR = -100;
				if (outL > 100) outL = 100;
				if (outL < -100) outL = -100;
				
				/* Stop if close enough to target (e.g. 1cm) */
				if (error > -0.01f && error < 0.01f) {
					outR = 0;
					outL = 0;
				}
				
				USER_Motor_SetSpeed(MOTOR_FR, (int8_t)outR);
				USER_Motor_SetSpeed(MOTOR_BR, (int8_t)outR);
				USER_Motor_SetSpeed(MOTOR_FL, (int8_t)outL);
				USER_Motor_SetSpeed(MOTOR_BL, (int8_t)outL);
			}
		}
	}
}

/**
 * vRemoteTask - Parses ESP32 UART string
 */
void vRemoteTask(void *pvParameters)
{
	(void)pvParameters;
	
	while(1) {
		if (USER_UART_RxReady) {
			USER_UART_RxReady = 0;
			char *p = USER_UART_RxBuffer;
			RemoteCommand_T cmd = {0};
			
			cmd.mode = atoi(p);
			while (*p && *p != ',') p++;
			if (*p == ',') {
				p++;
				cmd.throttle = atof(p);
				while (*p && *p != ',') p++;
				if (*p == ',') {
					p++;
					cmd.distance = atof(p);
					while (*p && *p != ',') p++;
					if (*p == ',') {
						p++;
						cmd.brake = atoi(p);
						
						/* Send to remote queue */
						xQueueOverwrite(xRemoteQueue, &cmd);
					}
				}
			}
		}
		vTaskDelay(pdMS_TO_TICKS(10));
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
	
	xRemoteQueue = xQueueCreate(1, sizeof(RemoteCommand_T));
	configASSERT(xRemoteQueue != NULL);
	
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
	
	/* Remote Parse Task - High priority (3), 10ms poll */
	xTaskCreate(
		vRemoteTask,
		"Remote",
		512,
		NULL,
		3,
		NULL
	);
	
	/* ===== Queue Registry (for debugger) ===== */
	vQueueAddToRegistry(xThrottleQueue, "ThrottleQ");
	vQueueAddToRegistry(xBrakeQueue, "BrakeQ");
	vQueueAddToRegistry(xTelemetryOutputQueue, "TelemetryOutQ");
	vQueueAddToRegistry(xLCDOutputQueue, "LCDOutQ");
	vQueueAddToRegistry(xRemoteQueue, "RemoteQ");
}
