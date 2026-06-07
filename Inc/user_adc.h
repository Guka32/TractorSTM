#ifndef USER_ADC_H_
#define USER_ADC_H_

#include <stdint.h>

void USER_ADC1_Init(void);
uint16_t USER_ADC1_ReadThrottleRaw(void);

#endif /* USER_ADC_H_ */