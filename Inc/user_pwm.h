#ifndef USER_PWM_H_
#define USER_PWM_H_

#include <stdint.h>

void USER_PWM4_Init(void);
void USER_PWM4_SetDutyPercent(uint8_t dutyPercent);

#endif /* USER_PWM_H_ */