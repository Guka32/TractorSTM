/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Auto-generated for Motor Control Project
 * @board          : NUCLEO-F103RB
 ******************************************************************************
 *
 * C code for motor control application using UART communication
 * Bare metal STM32F103RB implementation
 *
 ******************************************************************************
 */

/* Libraries, Definitions and Global Declarations */
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
#include "FreeRTOS.h"
#include "task.h"
#include "tasks.h"

static uint8_t USER_ReadBrakeState(void);
static uint8_t USER_ClampPercentFromRaw(uint16_t rawValue);
static uint16_t USER_ClampRpm(real_T rpmValue);
static uint16_t USER_ClampSpeed(real_T speedValue);
static void USER_LCD_WritePadded(uint8_t line, uint8_t column, const char *text, uint8_t fieldWidth);
static void USER_LCD_UpdateStatus(uint16_t engineRpm, uint16_t vehicleSpeed, uint8_t gear);

/* Debug globals for model tracing */
volatile real_T USER_Debug_ModelEngineSpeed = 0.0;
volatile real_T USER_Debug_ModelVehicleSpeed = 0.0;
volatile real_T USER_Debug_ModelGear = 0.0;
volatile uint16_t USER_Debug_ClampedEngineRpm = 0U;
volatile uint16_t USER_Debug_ClampedVehicleSpeed = 0U;
volatile uint8_t USER_Debug_ClampedGear = 0U;

/* Superloop structure - now replaced with FreeRTOS */
int main(void)
{
	/* Declarations and Initializations */
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

	__asm volatile ("cpsie i");

	USER_USART2_SendString("====================================\r\n");
	USER_USART2_SendString("TRANSMISSION CONTROL SYSTEM - STM32F103RB\r\n");
	USER_USART2_SendString("====================================\r\n");
	USER_USART2_SendString("System initialized successfully!\r\n");
	USER_USART2_SendString("Ready for FreeRTOS-based control loop.\r\n");
	USER_USART2_SendString("====================================\r\n\r\n");

	/* ===== FreeRTOS Task Creation ===== */
	vCreateAllTasks();

	/* ===== Start FreeRTOS Scheduler ===== */
	/* This function should never return */
	vTaskStartScheduler();

	/* Execution should never reach here */
	while(1) {
		/* Halt if scheduler exits */
	}

	return 0;
}

void USER_GPIO_Init( void ){
RCC->APB2ENR|= ( 0x1UL <<  2U ) |  ( 0x1UL <<  4U );//GPIOA and GPIOC clock enable

/* USER button on PC13 as input pull-up. */
GPIOC->CRH&=~( 0xFUL << 20U );
GPIOC->CRH|= ( 0x8UL << 20U );
GPIOC->ODR|= ( 0x1UL << 13U );
}

void USER_SystemClock_Config( void ){
FLASH->ACR&=~( 0x5UL <<  0U );//two wait states latency, if SYSCLK > 48MHz
FLASH->ACR|= ( 0x2UL <<  0U );//two wait states latency, if SYSCLK > 48MHz
RCC->CFGR&=~( 0x1UL << 16U )//PLL HSI oscillator clock /2 selected as PLL input clock
&~( 0x7UL << 11U )// APB2 prescaler /1
&~( 0x3UL <<  8U );// APB1 prescaler /2
RCC->CFGR|= ( 0xFUL << 18U )//PLL input clock x 16 (PLLMUL bits)
| ( 0x4UL <<  8U );//APB1 prescaler /2
RCC->CR|= ( 0x1UL << 24U );//PLL2 ON
while( !( RCC->CR & ( 0x1UL << 25U ) ) );//wait until PLL is locked
RCC->CFGR&=~( 0x1UL << 0U  );//PLL used as system clock (SW bits)
RCC->CFGR|= ( 0x2UL << 0U  );//PLL used as system clock (SW bits)
while( 0x8UL != ( RCC->CFGR & 0xCUL ));//wait until PLL is switched
}

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

	if (fieldWidth > 16U) {
		fieldWidth = 16U;
	}

	while ((i < fieldWidth) && (text[i] != '\0')) {
		field[i] = text[i];
		i++;
	}
	while (i < fieldWidth) {
		field[i++] = ' ';
	}
	field[fieldWidth] = '\0';

	LCD_Set_Cursor(line, column);
	LCD_Put_Str(field);
}

static void USER_LCD_UpdateStatus(uint16_t engineRpm, uint16_t vehicleSpeed, uint8_t gear)
{
	char speedText[16];
	char rpmText[16];
	char gearText[16];

	snprintf(speedText, sizeof speedText, "V:%u", (unsigned)vehicleSpeed);
	snprintf(rpmText, sizeof rpmText, "RPM:%u", (unsigned)engineRpm);
	snprintf(gearText, sizeof gearText, "G:%u", (unsigned)gear);

	USER_LCD_WritePadded(1U, 1U, speedText, 8U);
	USER_LCD_WritePadded(1U, 9U, rpmText, 8U);
	USER_LCD_WritePadded(2U, 1U, gearText, 8U);
	USER_LCD_WritePadded(2U, 9U, "", 8U);
}

static void USER_LCD_SimpleTest(void)
{
	/* Simple LCD test: write plain ASCII characters without snprintf */
	LCD_Set_Cursor(1U, 1U);
	LCD_Put_Char('A');
	LCD_Put_Char('B');
	LCD_Put_Char('C');
	LCD_Put_Char('1');
	LCD_Put_Char('2');
	LCD_Put_Char('3');

	LCD_Set_Cursor(2U, 1U);
	LCD_Put_Char('X');
	LCD_Put_Char('Y');
	LCD_Put_Char('Z');
	LCD_Put_Char('9');
	LCD_Put_Char('8');
	LCD_Put_Char('7');
}
