#ifndef USER_TIMER_H_
#define USER_TIMER_H_

#include <stdint.h>

void USER_TIM2_Init40ms(void);
uint8_t USER_TIM2_ConsumeTick(void);

#endif /* USER_TIMER_H_ */