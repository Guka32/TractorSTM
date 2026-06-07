#include <stdint.h>
#include "main.h"
#include "user_pwm.h"

#define TIM_CR1_CEN         (1UL << 0U)
#define TIM_CR1_ARPE        (1UL << 7U)
#define TIM_CCMR_OCxPE      (1UL << 3U)
#define TIM_CCMR_OCxM_PWM1  (0x6UL << 4U)

static uint16_t USER_PWM4_ClampDuty(uint8_t dutyPercent);
static void USER_PWM4_ApplyDuty(uint16_t compareValue);

void USER_PWM4_Init(void)
{
	RCC->APB2ENR |= (1UL << 0U) | (1UL << 2U) | (1UL << 3U);
	RCC->APB1ENR |= (1UL << 1U);

	GPIOA->CRL &= ~((0xFUL << 24U) | (0xFUL << 28U));
	GPIOA->CRL |= ((0xAUL << 24U) | (0xAUL << 28U));

	GPIOB->CRL &= ~((0xFUL << 0U) | (0xFUL << 4U));
	GPIOB->CRL |= ((0xAUL << 0U) | (0xAUL << 4U));

	TIM3->PSC = 255U;
	TIM3->ARR = 255U;
	TIM3->CCR1 = 0U;
	TIM3->CCR2 = 0U;
	TIM3->CCR3 = 0U;
	TIM3->CCR4 = 0U;
	TIM3->CCMR1 = (TIM_CCMR_OCxM_PWM1 | TIM_CCMR_OCxPE) |
	              ((TIM_CCMR_OCxM_PWM1 | TIM_CCMR_OCxPE) << 8U);
	TIM3->CCMR2 = (TIM_CCMR_OCxM_PWM1 | TIM_CCMR_OCxPE) |
	              ((TIM_CCMR_OCxM_PWM1 | TIM_CCMR_OCxPE) << 8U);
	TIM3->CCER = (1UL << 0U) | (1UL << 4U) | (1UL << 8U) | (1UL << 12U);
	TIM3->CR1 = TIM_CR1_ARPE;
	TIM3->EGR = 1U;
	TIM3->CR1 |= TIM_CR1_CEN;
}

void USER_PWM4_SetDutyPercent(uint8_t dutyPercent)
{
	uint16_t compareValue = USER_PWM4_ClampDuty(dutyPercent);
	USER_PWM4_ApplyDuty(compareValue);
}

static uint16_t USER_PWM4_ClampDuty(uint8_t dutyPercent)
{
	if (dutyPercent > 100U) {
		dutyPercent = 100U;
	}

	return (uint16_t)(((uint32_t)dutyPercent * 255U) / 100U);
}

static void USER_PWM4_ApplyDuty(uint16_t compareValue)
{
	/* TIM3 CCR1-4 are exclusively controlled by user_motor.c (USER_Motor_SetSpeed).
	 * Writing here would overwrite per-motor PWM values and cause all motors
	 * to run at the same speed (the LED duty). This function is intentionally
	 * left as a no-op. USER_PWM4_Init must still run to configure TIM3 + GPIO. */
	(void)compareValue;
}