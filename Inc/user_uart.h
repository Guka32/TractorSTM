#ifndef USER_UART_H_
#define USER_UART_H_

#include <stdio.h>
#include <stdint.h>

int _write(int file, char *ptr, int len);
#define USARTDIV        0xD05//				    9600 baud rate at 32 MHz (USART2 on APB1)
#define USART_CR1_UE    ( 0x1UL << 13U )
#define USART_CR1_M     ( 0x1UL << 12U )
#define USART_CR1_TE    ( 0x1UL <<  3U )
#define USART_CR1_RE    ( 0x1UL <<  2U )
#define USART_CR2_STOP  ( 0x3UL << 12U )
#define USART_SR_TXE    ( 0x1UL <<  7U )
#define USART_SR_RXNE   ( 0x1UL <<  5U )

void USER_USART2_Init( void );
void USER_USART2_Transmit( uint8_t *pData, uint16_t size );
uint8_t USER_USART2_Receive_8bit( void );
void USER_GPIO_LED_Init( void );
void USER_USART2_SendString(const char *text);
void USER_USART2_SendTelemetry(uint16_t throttlePercent, uint8_t brakeActive, uint16_t engineRpm, uint16_t vehicleSpeed, uint8_t gear);

extern char USER_UART_RxBuffer[64];
extern volatile uint8_t USER_UART_RxReady;

#endif /* USER_UART_H_ */
