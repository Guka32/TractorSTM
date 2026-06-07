/**
 ******************************************************************************
 * @file       user_motor.c
 * @brief      Motor control driver implementation for STM32F103RB
 ******************************************************************************
 */

#include <stdint.h>
#include "main.h"
#include "user_motor.h"

/* Direction Pins
 * FR: PC1, PC2
 * BR: PC3, PC4
 * FL: PC5, PB2
 * BL: PB3, PB4
 * STBY: PC0
 */

#define PIN_FR_IN1  (1UL << 1U) // PC1
#define PIN_FR_IN2  (1UL << 2U) // PC2
#define PIN_BR_IN1  (1UL << 3U) // PC3
#define PIN_BR_IN2  (1UL << 4U) // PC4
#define PIN_FL_IN1  (1UL << 5U) // PC5
#define PIN_STBY    (1UL << 0U) // PC0

#define PIN_FL_IN2  (1UL << 2U) // PB2
#define PIN_BL_IN1  (1UL << 3U) // PB3
#define PIN_BL_IN2  (1UL << 4U) // PB4

static int16_t lastRightEncoder = 0;
static int16_t lastLeftEncoder = 0;

/**
 * @brief Initialize motor control GPIO, PWM, and Encoder Timers
 */
void USER_Motor_Init(void)
{
	/* Enable clocks for GPIOB, GPIOC, TIM1, TIM4 */
	RCC->APB2ENR |= (1UL << 3U) | (1UL << 4U) | (1UL << 11U); // GPIOB, GPIOC, TIM1
	RCC->APB1ENR |= (1UL << 2U); // TIM4

	/* Configure STBY (PC0), FR_IN1 (PC1), FR_IN2 (PC2), BR_IN1 (PC3), BR_IN2 (PC4), FL_IN1 (PC5) as Push-Pull Output (50MHz) */
	GPIOC->CRL &= ~((0xFUL << 0U) | (0xFUL << 4U) | (0xFUL << 8U) | (0xFUL << 12U) | (0xFUL << 16U) | (0xFUL << 20U));
	GPIOC->CRL |= ((0x3UL << 0U) | (0x3UL << 4U) | (0x3UL << 8U) | (0x3UL << 12U) | (0x3UL << 16U) | (0x3UL << 20U));

	/* Configure FL_IN2 (PB2), BL_IN1 (PB3), BL_IN2 (PB4) as Push-Pull Output (50MHz) */
	GPIOB->CRL &= ~((0xFUL << 8U) | (0xFUL << 12U) | (0xFUL << 16U));
	GPIOB->CRL |= ((0x3UL << 8U) | (0x3UL << 12U) | (0x3UL << 16U));

	/* Set all directions to 0 initially */
	GPIOC->BRR = PIN_FR_IN1 | PIN_FR_IN2 | PIN_BR_IN1 | PIN_BR_IN2 | PIN_FL_IN1;
	GPIOB->BRR = PIN_FL_IN2 | PIN_BL_IN1 | PIN_BL_IN2;

	/* Start disabled */
	USER_Motor_Enable(0);

	/* --- Encoder Configuration --- */
	/* Right Encoder: TIM1_CH1 (PA8), TIM1_CH2 (PA9) */
	/* Left Encoder: TIM4_CH1 (PB6), TIM4_CH2 (PB7) */

	/* Ensure GPIOA and GPIOB clocks are enabled */
	RCC->APB2ENR |= (1UL << 2U) | (1UL << 3U);

	/* PA8, PA9 as Floating Input (Mode 0, CNF 1) */
	GPIOA->CRH &= ~((0xFUL << 0U) | (0xFUL << 4U));
	GPIOA->CRH |= ((0x4UL << 0U) | (0x4UL << 4U));

	/* PB6, PB7 as Floating Input */
	GPIOB->CRL &= ~((0xFUL << 24U) | (0xFUL << 28U));
	GPIOB->CRL |= ((0x4UL << 24U) | (0x4UL << 28U));

	/* Configure TIM1 for Encoder Mode 3 */
	TIM1->CCMR1 = (1UL << 0U) | (1UL << 8U); /* CC1S=01 (TI1), CC2S=01 (TI2) */
	TIM1->CCER = 0; /* Non-inverted */
	TIM1->SMCR = (3UL << 0U); /* SMS=011 (Encoder mode 3) */
	TIM1->ARR = 0xFFFF;
	TIM1->CR1 |= (1UL << 0U); /* CEN */

	/* Configure TIM4 for Encoder Mode 3 */
	TIM4->CCMR1 = (1UL << 0U) | (1UL << 8U); /* CC1S=01 (TI1), CC2S=01 (TI2) */
	TIM4->CCER = 0; /* Non-inverted */
	TIM4->SMCR = (3UL << 0U); /* SMS=011 (Encoder mode 3) */
	TIM4->ARR = 0xFFFF;
	TIM4->CR1 |= (1UL << 0U); /* CEN */

	lastRightEncoder = 0;
	lastLeftEncoder = 0;
}

/**
 * @brief Set motor speed (-100 to 100%)
 */
void USER_Motor_SetSpeed(MotorID_t motor, int8_t speed)
{
	uint8_t dir = 1; /* 1 = forward, 0 = backward */
	if (speed < 0) {
		dir = 0;
		speed = -speed;
	}
	if (speed > 100) speed = 100;

	uint16_t compareValue = (uint16_t)(((uint32_t)speed * 255U) / 100U);

	switch (motor) {
		case MOTOR_FR:
			if (dir) { GPIOC->BSRR = PIN_FR_IN1; GPIOC->BRR = PIN_FR_IN2; }
			else     { GPIOC->BRR = PIN_FR_IN1; GPIOC->BSRR = PIN_FR_IN2; }
			if (speed == 0) { GPIOC->BRR = PIN_FR_IN1 | PIN_FR_IN2; }
			TIM3->CCR1 = compareValue;
			break;
		case MOTOR_BR:
			if (dir) { GPIOC->BSRR = PIN_BR_IN1; GPIOC->BRR = PIN_BR_IN2; }
			else     { GPIOC->BRR = PIN_BR_IN1; GPIOC->BSRR = PIN_BR_IN2; }
			if (speed == 0) { GPIOC->BRR = PIN_BR_IN1 | PIN_BR_IN2; }
			TIM3->CCR2 = compareValue;
			break;
		case MOTOR_FL:
			if (dir) { GPIOC->BSRR = PIN_FL_IN1; GPIOB->BRR = PIN_FL_IN2; }
			else     { GPIOC->BRR = PIN_FL_IN1; GPIOB->BSRR = PIN_FL_IN2; }
			if (speed == 0) { GPIOC->BRR = PIN_FL_IN1; GPIOB->BRR = PIN_FL_IN2; }
			TIM3->CCR3 = compareValue;
			break;
		case MOTOR_BL:
			if (dir) { GPIOB->BSRR = PIN_BL_IN1; GPIOB->BRR = PIN_BL_IN2; }
			else     { GPIOB->BRR = PIN_BL_IN1; GPIOB->BSRR = PIN_BL_IN2; }
			if (speed == 0) { GPIOB->BRR = PIN_BL_IN1 | PIN_BL_IN2; }
			TIM3->CCR4 = compareValue;
			break;
		default:
			break;
	}
}

/**
 * @brief Enable/disable motor
 */
void USER_Motor_Enable(uint8_t enable)
{
	if (enable) {
		GPIOC->BSRR = PIN_STBY;
	} else {
		GPIOC->BRR = PIN_STBY;
	}
}

/**
 * @brief Read encoder deltas
 */
void USER_Motor_ReadEncoders(int16_t *rightDelta, int16_t *leftDelta)
{
	int16_t currentRight = (int16_t)TIM1->CNT;
	int16_t currentLeft = (int16_t)TIM4->CNT;

	if (rightDelta) {
		*rightDelta = currentRight - lastRightEncoder;
		lastRightEncoder = currentRight;
	}
	if (leftDelta) {
		*leftDelta = currentLeft - lastLeftEncoder;
		lastLeftEncoder = currentLeft;
	}
}
