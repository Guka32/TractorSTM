/*
 * tasks.h
 * FreeRTOS task definitions for tractor control system
 */

#ifndef TASKS_H_
#define TASKS_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "EngTrModel.h"

/* ============================================================================
 * Shared Data Structures
 * ============================================================================ */

/* Model outputs passed between Control and Telemetry tasks */
typedef struct {
	real_T engineSpeed;			/* RPM from model */
	real_T vehicleSpeed;		/* Speed from model */
	uint8_t gear;				/* Current gear */
	uint8_t throttlePercent;	/* 0-100% */
	uint8_t brakeActive;		/* 0 or 1 */
} ModelOutputs_T;

/* Structure for Remote Control Commands */
typedef struct {
	int mode;         /* 1: Teleop, 2: Distance */
	float throttle;   /* Throttle percent */
	float distance;   /* Distance to travel (m) */
	int brake;        /* Brake active (1/0) */
} RemoteCommand_T;

/* ============================================================================
 * Task Function Declarations
 * ============================================================================ */

/**
 * @brief Control Task - Highest priority, runs at 40ms triggered by TIM2
 * 
 * Responsibilities:
 * - Wait for TIM2 ISR notification (40ms tick)
 * - Receive throttle/brake values from ADC task queue
 * - Run EngTrModel_step()
 * - Update LED PWM based on throttle
 * - Send model outputs to Telemetry task queue
 * 
 * @param pvParameters: not used
 */
void vControlTask(void *pvParameters);

/**
 * @brief ADC Task - Medium priority, 40ms periodic
 * 
 * Responsibilities:
 * - Read throttle potentiometer (PA0)
 * - Read brake button (PC13)
 * - Apply smoothing/ramping
 * - Send values to Control task queue
 * 
 * @param pvParameters: not used
 */
void vADCTask(void *pvParameters);

/**
 * @brief Telemetry Task - Low-medium priority, event-driven
 * 
 * Responsibilities:
 * - Wait for model outputs from Control task
 * - Format and send CSV telemetry to ESP32 UART
 * 
 * @param pvParameters: not used
 */
void vTelemetryTask(void *pvParameters);

/**
 * @brief LCD Update Task - Low priority, 200ms periodic
 * 
 * Responsibilities:
 * - Update HD44780 display every 200ms
 * - Display speed, RPM, gear
 * 
 * @param pvParameters: not used
 */
void vLCDTask(void *pvParameters);

/**
 * @brief Motor Control Task - Highest priority (for future use), 10ms periodic
 * 
 * Responsibilities:
 * - Read encoder feedback from motors (phase 2)
 * - Compute PID control loops (phase 2)
 * - Drive motor PWM outputs
 * - Currently a stub
 * 
 * @param pvParameters: not used
 */
void vMotorTask(void *pvParameters);

/* ============================================================================
 * Queue Handles (created in main, used by tasks)
 * ============================================================================ */

extern QueueHandle_t xThrottleQueue;	 /* uint8_t throttle % */
extern QueueHandle_t xBrakeQueue;	 /* uint8_t brake (0 or 1) */
extern QueueHandle_t xTelemetryOutputQueue; /* ModelOutputs_T for Telemetry */
extern QueueHandle_t xLCDOutputQueue;       /* ModelOutputs_T for LCD */
extern QueueHandle_t xRemoteQueue;          /* RemoteCommand_T from ESP32 */

/* ============================================================================
 * Task Handles (for ISR notifications, etc.)
 * ============================================================================ */

extern TaskHandle_t xControlTaskHandle;
extern TaskHandle_t xADCTaskHandle;

/* ============================================================================
 * Initialization Function
 * ============================================================================ */

/**
 * @brief Create all FreeRTOS tasks and queues
 * 
 * Called from main() after peripheral initialization
 * and before vTaskStartScheduler()
 */
void vCreateAllTasks(void);

#endif /* TASKS_H_ */
