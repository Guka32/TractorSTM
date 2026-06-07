#ifndef USER_MOTOR_H_
#define USER_MOTOR_H_

#include <stdint.h>

/*
 ******************************************************************************
 * @file       user_motor.h
 * @brief      Motor control driver for STM32F103RB
 ******************************************************************************
 */

/* Motor Enums */
typedef enum {
    MOTOR_FR = 0,
    MOTOR_BR,
    MOTOR_FL,
    MOTOR_BL,
    MOTOR_COUNT
} MotorID_t;

/* Motor control function prototypes */

/**
 * @brief Initialize motor control GPIO, PWM, and Encoder Timers
 */
void USER_Motor_Init(void);

/**
 * @brief Set motor speed (-100 to 100%)
 * @param motor: Motor ID
 * @param speed: Speed percentage (-100 to 100, where negative is reverse)
 */
void USER_Motor_SetSpeed(MotorID_t motor, int8_t speed);

/**
 * @brief Enable/disable all motors via STBY pin
 * @param enable: 1 to enable, 0 to disable
 */
void USER_Motor_Enable(uint8_t enable);

/**
 * @brief Read encoder delta since last call
 * @param rightDelta: Pointer to store right side delta
 * @param leftDelta: Pointer to store left side delta
 */
void USER_Motor_ReadEncoders(int16_t *rightDelta, int16_t *leftDelta);

#endif /* USER_MOTOR_H_ */
