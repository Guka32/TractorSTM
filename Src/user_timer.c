#include <stdint.h>
#include "main.h"
#include "user_timer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tasks.h"

#define TIM_CR1_CEN         (1UL << 0U)
#define TIM_DIER_UIE        (1UL << 0U)
#define TIM_SR_UIF          (1UL << 0U)

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
	
	/* Set TIM2 interrupt priority to 5 via NVIC IPR register */
	/* NVIC IPR base address: 0xE000E400, indexed by IRQn */
	volatile uint8_t *nvic_ipr = (volatile uint8_t *)0xE000E400;
	nvic_ipr[TIM2_IRQn] = (5U << 4U);
	
	NVIC_ISER0 |= (1UL << TIM2_IRQn);
}

void TIM2_IRQHandler(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	if ((TIM2->SR & TIM_SR_UIF) != 0U) {
		TIM2->SR = 0U;
		
		/* Notify Control task that 40ms tick has occurred */
		if (xControlTaskHandle != NULL) {
			vTaskNotifyGiveFromISR(xControlTaskHandle, &xHigherPriorityTaskWoken);
		}
		
		/* Yield to higher priority task if one was woken */
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}