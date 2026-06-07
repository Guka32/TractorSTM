#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "user_uart.h"

static void USER_USART2_Send_8bit( uint8_t Data );

/* Initialize GPIO for LED (PA5) */
void USER_GPIO_LED_Init( void ){
RCC->APB2ENR|= ( 0x1UL <<  2U );//IO port A clock enable
//pin PA5 (LED) as output push-pull, max speed 10MHz
GPIOA->CRL&=~( 0xFUL << 20U );//Clear PA5 bits
GPIOA->CRL|= ( 0x1UL << 20U );//PA5 Output 10MHz
}

int _write(int file, char *ptr, int len){
(void)file;
int DataIdx;
for(DataIdx=0; DataIdx<len; DataIdx++){
while(!( USART2->SR & USART_SR_TXE ));
USART2->DR = *ptr++;
}
return len;
}

void USER_USART2_Init( void ){
RCC->APB1ENR|= ( 0x1UL << 17U );//USART2 clock enable
RCC->APB2ENR|= ( 0x1UL <<  2U );//IO port A clock enable
USART2->CR1|= USART_CR1_UE;//Step 1 Usart enabled
USART2->CR1&=~USART_CR1_M;//Step 2 8 Data bits
USART2->CR2&=~USART_CR2_STOP;//Step 3 1 Stop bit
USART2->BRR  = USARTDIV;//Step 5 Desired baud rate
	USART2->CR1|=  USART_CR1_TE;//Step 6 Transmitter enabled
	USART2->CR1|=  USART_CR1_RE;//Step 6b Receiver enabled

	/* NOTE: RXNE interrupt is NOT enabled here.
	 * It will be enabled by USER_USART2_EnableRX() once the UART cable
	 * is physically connected to the ESP32. Without this, a floating
	 * PA3 pin generates noise that corrupts the remote command queue. */

	/* Configure PA2 (TX) as Alternate Function Output Push-Pull */
	uint32_t temp = GPIOA->CRL;
	temp &= ~( 0xFUL << (2U * 4U));//Clear PA2 bits
	temp |= (0xAUL << (2U * 4U));//PA2 AF Push-Pull Output 10MHz (0xA)
	GPIOA->CRL = temp;

	/* Configure PA3 (RX) as Input with Pull-Down.
	 * CNF = 10 (input with pull-up/pull-down), MODE = 00.
	 * Setting ODR bit LOW selects pull-down.
	 * This holds the line stable at 0 when no UART cable is connected
	 * and prevents false RXNE interrupts from floating-pin noise. */
	temp = GPIOA->CRL;
	temp &= ~( 0xFUL << (3U * 4U)); /* Clear PA3 bits */
	temp |= (0x8UL << (3U * 4U));  /* PA3: Input with pull (CNF=10, MODE=00) */
	GPIOA->CRL = temp;
	GPIOA->ODR &= ~(1UL << 3U);    /* Pull-DOWN: ODR bit = 0 */
}

/**
 * @brief Enable UART2 RX interrupt.
 * Call this only after the ESP32 UART cable has been physically connected.
 * Calling it with a floating/unconnected PA3 will cause motor twitching.
 */
void USER_USART2_EnableRX(void)
{
	/* Enable RXNE interrupt */
	USART2->CR1 |= (1UL << 5U); /* RXNEIE */

	/* Configure NVIC for USART2 (IRQ 38) */
	volatile uint8_t *nvic_ipr = (volatile uint8_t *)0xE000E400;
	nvic_ipr[38] = (6U << 4U); /* Priority 6 */
	NVIC_ISER1 |= (1UL << (38 - 32));
}


void USER_USART2_Transmit( uint8_t *pData, uint16_t size ){
for( int i = 0; i < size; i++ ){
USER_USART2_Send_8bit( *pData++ );
}
}

static void USER_USART2_Send_8bit( uint8_t Data ){
while(!( USART2->SR & USART_SR_TXE ));//wait until next data can be written
USART2->DR = Data;//Step 7 Data to send
}

/* Receive 8-bit data via USART2 */
uint8_t USER_USART2_Receive_8bit( void ){
while(!( USART2->SR & USART_SR_RXNE ));//wait until a data is received (RXNE flag)
return (uint8_t)(USART2->DR & 0xFF);//return data (DR register)
}

void USER_USART2_SendString(const char *text)
{
if (text == NULL) {
return;
}

USER_USART2_Transmit((uint8_t *)text, (uint16_t)strlen(text));
}

void USER_USART2_SendTelemetry(uint16_t throttlePercent, uint8_t brakeActive, uint16_t engineRpm, uint16_t vehicleSpeed, uint8_t gear)
{
char telemetryFrame[96];
int written = snprintf(
telemetryFrame,
sizeof telemetryFrame,
"%u,%u,%u,%u,%u\r\n",
(unsigned int)throttlePercent,
(unsigned int)brakeActive,
(unsigned int)engineRpm,
(unsigned int)vehicleSpeed,
(unsigned int)gear);

	if (written > 0) {
		uint16_t frameSize = (written >= (int)sizeof telemetryFrame) ? (uint16_t)(sizeof telemetryFrame - 1U) : (uint16_t)written;
		USER_USART2_Transmit((uint8_t *)telemetryFrame, frameSize);
	}
}

/* ISR for USART2 RX */
char USER_UART_RxBuffer[64];
volatile uint8_t USER_UART_RxReady = 0;
static uint8_t rx_idx = 0;

void USART2_IRQHandler(void)
{
	if (USART2->SR & USART_SR_RXNE) {
		char c = (char)(USART2->DR & 0xFF);
		if (c == '\n' || c == '\r') {
			if (rx_idx > 0) {
				USER_UART_RxBuffer[rx_idx] = '\0';
				USER_UART_RxReady = 1;
				rx_idx = 0;
			}
		} else {
			if (rx_idx < sizeof(USER_UART_RxBuffer) - 1) {
				USER_UART_RxBuffer[rx_idx++] = c;
			}
		}
	}
}
