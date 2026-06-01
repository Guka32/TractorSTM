#include <stdint.h>
#include "main.h"
#include "user_adc.h"

#define ADC_CR2_ADON        (1UL << 0U)
#define ADC_CR2_CAL         (1UL << 2U)
#define ADC_CR2_RSTCAL      (1UL << 3U)
#define ADC_CR2_EXTTRIG     (1UL << 20U)
#define ADC_CR2_SWSTART     (1UL << 22U)
#define ADC_CR2_EXTSEL      (0x7UL << 17U)
#define ADC_SR_EOC          (1UL << 1U)

static void USER_ADC1_StartConversion(void);

void USER_ADC1_Init(void)
{
	RCC->APB2ENR |= (1UL << 2U) | (1UL << 9U);

	RCC->CFGR &= ~(0x3UL << 14U);
	RCC->CFGR |= (0x2UL << 14U);

	GPIOA->CRL &= ~(0xFUL << 0U);

	ADC1->CR2 &= ~ADC_CR2_ADON;
	ADC1->CR1 = 0U;
	ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL;
	ADC1->SMPR2 &= ~(0x7UL << 0U);
	ADC1->SMPR2 |= (0x7UL << 0U);
	ADC1->SQR1 &= ~(0xFUL << 20U);
	ADC1->SQR3 &= ~(0x1FUL << 0U);

	ADC1->CR2 |= ADC_CR2_RSTCAL;
	while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0U) {
	}

	ADC1->CR2 |= ADC_CR2_CAL;
	while ((ADC1->CR2 & ADC_CR2_CAL) != 0U) {
	}
}

uint16_t USER_ADC1_ReadThrottleRaw(void)
{
	USER_ADC1_StartConversion();
	while ((ADC1->SR & ADC_SR_EOC) == 0U) {
	}
	return (uint16_t)(ADC1->DR & 0x0FFFU);
}

static void USER_ADC1_StartConversion(void)
{
	ADC1->CR2 |= ADC_CR2_SWSTART;
}