/* Minimal HD44780 4-bit driver adapted for Tractor project
 * Uses busy-flag disabled (RW assumed tied low) and FreeRTOS vTaskDelay for timing.
 */
#include <stdint.h>
#include "main.h"
#include "lcd.h"
#include "FreeRTOS.h"
#include "task.h"

/* Simple busy-wait for sub-millisecond delays (loops at 64 MHz) */
static void LCD_DelayUs(uint32_t microseconds)
{
	volatile uint32_t cycles = microseconds * 64U / 3U;  /* ~3 cycles per loop at -O0 */
	while (cycles--);
}

static void LCD_Delay_10us(void)   { LCD_DelayUs(10U); }
static void LCD_Delay_53us(void)   { LCD_DelayUs(53U); }
static void LCD_Delay_100us(void)  { LCD_DelayUs(100U); }
static void LCD_Delay_1ms(void)    { if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(pdMS_TO_TICKS(1)); else LCD_DelayUs(1000U); }
static void LCD_Delay_4_1ms(void)  { if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(pdMS_TO_TICKS(5)); else LCD_DelayUs(4100U); }  /* Rounded up for safety */
static void LCD_Delay_40ms(void)   { if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(pdMS_TO_TICKS(40)); else LCD_DelayUs(40000U); }

/* Simple user font (kept from original) */
const int8_t UserFont[8][8] = {
    {0x11,0x0A,0x04,0x1B,0x11,0x11,0x11,0x0E},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10},
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},
    {0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C},
    {0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E},
    {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0}
};

/* Helper to pulse EN */
void LCD_Pulse_EN(void)
{
    GPIOC->BSRR = LCD_EN_PIN_LOW;
    LCD_Delay_10us();
    GPIOC->BSRR = LCD_EN_PIN_HIGH;
    LCD_Delay_100us();  /* Increased from 10us for data latch time */
    GPIOC->BSRR = LCD_EN_PIN_LOW;
    LCD_Delay_1ms();
}

/* Output 4 data bits (D4..D7) - atomically set/clear all bits */
void LCD_Out_Data4(uint8_t val)
{
    uint32_t bsrr_val = 0UL;
    
    /* Build BSRR value to set/clear all 4 bits at once */
    if (val & 0x01U) bsrr_val |= LCD_D4_PIN_HIGH; else bsrr_val |= LCD_D4_PIN_LOW;
    if (val & 0x02U) bsrr_val |= LCD_D5_PIN_HIGH; else bsrr_val |= LCD_D5_PIN_LOW;
    if (val & 0x04U) bsrr_val |= LCD_D6_PIN_HIGH; else bsrr_val |= LCD_D6_PIN_LOW;
    if (val & 0x08U) bsrr_val |= LCD_D7_PIN_HIGH; else bsrr_val |= LCD_D7_PIN_LOW;
    
    /* Write to BSRR once with all bits */
    GPIOC->BSRR = bsrr_val;
    
    LCD_DelayUs(1U);  /* Minimal setup time before EN pulse */
}

/* Write full byte (4-bit mode) */
void LCD_Write_Byte(uint8_t val)
{
    vTaskSuspendAll();
    LCD_Out_Data4((val >> 4) & 0x0FU);
    LCD_DelayUs(1U);  /* Setup time before EN pulse */
    LCD_Pulse_EN();
    LCD_Out_Data4(val & 0x0FU);
    LCD_DelayUs(1U);  /* Setup time before EN pulse */
    LCD_Pulse_EN();
    xTaskResumeAll();
    
    LCD_Delay_1ms();
}

void LCD_Write_Cmd(uint8_t val)
{
    GPIOC->BSRR = LCD_RS_PIN_LOW; /* command */
    LCD_DelayUs(1U);  /* Setup time for RS */
    LCD_Write_Byte(val);
    LCD_Delay_1ms();  /* Command execution time */
}

void LCD_Put_Char(uint8_t c)
{
    GPIOC->BSRR = LCD_RS_PIN_HIGH; /* data */
    LCD_DelayUs(1U);  /* Setup time for RS */
    LCD_Write_Byte(c);
    LCD_Delay_1ms();  /* Character write time */
}

void LCD_Set_Cursor(uint8_t line, uint8_t column)
{
    uint8_t address;
    if (line == 0) line = 1; if (column == 0) column = 1;
    column--; line--;
    address = (line * 0x40U) + column;
    LCD_Write_Cmd(0x80U | (address & 0x7FU));
    LCD_Delay_1ms();  /* Cursor positioning time */
}

void LCD_Put_Str(char *str)
{
    for (int16_t i = 0; i < 16 && str[i] != 0; i++) LCD_Put_Char((uint8_t)str[i]);
}

void LCD_Put_Num(int16_t num)
{
    char buf[6];
    int idx = 0;
    if (num == 0) { LCD_Put_Char('0'); return; }
    if (num < 0) { LCD_Put_Char('-'); num = -num; }
    while (num > 0 && idx < 5) { buf[idx++] = '0' + (num % 10); num /= 10; }
    for (int i = idx - 1; i >= 0; i--) LCD_Put_Char(buf[i]);
}

/* Simple busy check: disabled by default to avoid hardware read complexity */
char LCD_Busy(void)
{
    (void)LCD_Delay_10us;
    return 0;
}

void LCD_BarGraphic(int16_t value, int16_t size)
{
    value = value * size / 20;
    for (int16_t i = 0; i < size; i++){
        if (value > 5){ LCD_Put_Char(0x05U); value -= 5; }
        else { LCD_Put_Char((char)value); break; }
    }
}

void LCD_BarGraphicXY(int16_t pos_x, int16_t pos_y, int16_t value)
{
    LCD_Set_Cursor((uint8_t)pos_x, (uint8_t)pos_y);
    for (int16_t i = 0; i < 16; i++){
        if (value > 5){ LCD_Put_Char(0x05U); value -= 5; }
        else { LCD_Put_Char((char)value); while (i++ < 16) LCD_Put_Char(0); }
    }
}

/* Initialize LCD pins and bring interface up */
void LCD_Init(void)
{
    /* Enable port C */
    RCC->APB2ENR |= (1UL << 4U);
    /* Configure PC6-PC12 as push-pull outputs, 10MHz (CRL/CRH) */
    /* PC6-PC7 in CRL (bits 24..31) */
    GPIOC->CRL &= ~((0xFUL << 24) | (0xFUL << 28));
    GPIOC->CRL |=  ((0x1UL << 24) | (0x1UL << 28));
    /* PC8-PC12 in CRH (bits 0..20) */
    GPIOC->CRH &= ~((0xFUL << 0) | (0xFUL << 4) | (0xFUL << 8) | (0xFUL << 12) | (0xFUL << 16));
    GPIOC->CRH |=  ((0x1UL << 0) | (0x1UL << 4) | (0x1UL << 8) | (0x1UL << 12) | (0x1UL << 16));

    /* Drive control lines low and wait */
    GPIOC->BSRR = LCD_RS_PIN_LOW;
    GPIOC->BSRR = LCD_RW_PIN_LOW;
    GPIOC->BSRR = LCD_EN_PIN_LOW;
    GPIOC->BSRR = LCD_D4_PIN_LOW;
    GPIOC->BSRR = LCD_D5_PIN_LOW;
    GPIOC->BSRR = LCD_D6_PIN_LOW;
    GPIOC->BSRR = LCD_D7_PIN_LOW;
    LCD_Delay_40ms();

    /* Initialization sequence (4-bit) */
    GPIOC->BSRR = LCD_D4_PIN_HIGH; GPIOC->BSRR = LCD_D5_PIN_HIGH; GPIOC->BSRR = LCD_D6_PIN_LOW; GPIOC->BSRR = LCD_D7_PIN_LOW; LCD_Pulse_EN(); LCD_Delay_4_1ms();
    GPIOC->BSRR = LCD_D4_PIN_HIGH; GPIOC->BSRR = LCD_D5_PIN_HIGH; GPIOC->BSRR = LCD_D6_PIN_LOW; GPIOC->BSRR = LCD_D7_PIN_LOW; LCD_Pulse_EN(); LCD_Delay_53us();
    GPIOC->BSRR = LCD_D4_PIN_HIGH; GPIOC->BSRR = LCD_D5_PIN_HIGH; GPIOC->BSRR = LCD_D6_PIN_LOW; GPIOC->BSRR = LCD_D7_PIN_LOW; LCD_Pulse_EN();
    LCD_Delay_1ms();

    /* Enter 4-bit mode (Send 0x02) */
    GPIOC->BSRR = LCD_D4_PIN_LOW; GPIOC->BSRR = LCD_D5_PIN_HIGH; GPIOC->BSRR = LCD_D6_PIN_LOW; GPIOC->BSRR = LCD_D7_PIN_LOW; LCD_Pulse_EN();
    LCD_Delay_1ms();

    /* Set 4-bit mode and function */
    LCD_Write_Cmd(0x28); /* 2-line, 5x7 */
    LCD_Write_Cmd(0x08); /* display off */
    LCD_Write_Cmd(0x01); /* clear */
    LCD_Write_Cmd(0x06); /* entry mode */
    LCD_Write_Cmd(0x0C); /* display on, cursor off */
    LCD_Write_Cmd(0x0C); /* display on, cursor off */

    /* User font not required for plain numeric/status output */
}

