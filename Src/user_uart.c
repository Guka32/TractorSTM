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

/* Configure PA2 (TX) as Alternate Function Output Push-Pull */
uint32_t temp = GPIOA->CRL;
temp &= ~( 0xFUL << (2U * 4U));//Clear PA2 bits
temp |= (0xAUL << (2U * 4U));//PA2 AF Push-Pull Output 10MHz (0xA)
GPIOA->CRL = temp;

/* Configure PA3 (RX) as Alternate Function Input Floating */
temp = GPIOA->CRL;
temp &= ~( 0xFUL << (3U * 4U));//Clear PA3 bits
temp |= (0x4UL << (3U * 4U));//PA3 Floating Input (0x4 = mode 0, CNF 1)
GPIOA->CRL = temp;
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
"TEL,THR=%u,BRAKE=%u,RPM=%u,VS=%u,GEAR=%u\r\n",
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
