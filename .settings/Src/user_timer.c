#include <stdint.h>
#include "main.h"
#include "user_timer.h"

#define TIM_CR1_CEN         (1UL << 0U)
#define TIM_DIER_UIE        (1UL << 0U)
#define TIM_SR_UIF          (1UL << 0U)

static volatile uint8_t USER_TIM2_TickFlag = 0U;

void USER_TIM2_Init40ms(void)
{
	RCC->APB1ENR |= (1UL << 0U);
	TIM2->PSC = 63999U;
	TIM2->ARR = 39U;
	TIM2->CNT = 0U;
	TIM2->SR = 0U;
	TIM2->DIER |= TIM_DIER_UIE;
	TIM2->EGR = 1U;
	TIM2->CR1 |= TIM_CR1_CEN;
	NVIC_ISER0 |= (1UL << TIM2_IRQn);
}

uint8_t USER_TIM2_ConsumeTick(void)
{
	uint8_t tick = USER_TIM2_TickFlag;
	USER_TIM2_TickFlag = 0U;
	return tick;
}

void TIM2_IRQHandler(void)
{
	if ((TIM2->SR & TIM_SR_UIF) != 0U) {
		TIM2->SR = 0U;
		USER_TIM2_TickFlag = 1U;
	}
}