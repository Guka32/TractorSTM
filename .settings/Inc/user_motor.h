#ifndef USER_MOTOR_H_
#define USER_MOTOR_H_

#include <stdint.h>

/*
 ******************************************************************************
 * @file       user_motor.h
 * @brief      Motor control driver for STM32F103RB
 * 
 * DESCRIPTION:
 * This header defines the interface for motor control functionality.
 * The motor control module will handle:
 *  - Motor initialization and GPIO setup
 *  - PWM speed control
 *  - Direction control (forward/reverse)
 *  - Motor enable/disable
 * 
 * TODO: Add motor specifications and implement motor control functions
 * ******************************************************************************
 */

/* Motor control function prototypes (to be implemented) */

/**
 * @brief Initialize motor control GPIO and PWM
 * @note To be implemented with motor specifications
 */
void USER_Motor_Init(void);

/**
 * @brief Set motor speed (0-100%)
 * @param speed: Speed percentage (0-100)
 * @note To be implemented with motor specifications
 */
void USER_Motor_SetSpeed(uint8_t speed);

/**
 * @brief Set motor direction
 * @param direction: 1 for forward, 0 for reverse
 * @note To be implemented with motor specifications
 */
void USER_Motor_SetDirection(uint8_t direction);

/**
 * @brief Enable/disable motor
 * @param enable: 1 to enable, 0 to disable
 * @note To be implemented with motor specifications
 */
void USER_Motor_Enable(uint8_t enable);

#endif /* USER_MOTOR_H_ */
