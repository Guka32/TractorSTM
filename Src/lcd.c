/* Minimal HD44780 4-bit driver adapted for Tractor project
 * Uses busy-flag disabled (RW assumed tied low) and DWT cycle counter for delays.
 */
#include <stdint.h>
#include "main.h"
#include "lcd.h"

/* DWT (Data Watchpoint and Trace) structures for cycle counter access */
#define DWT_CYCCNT (*((volatile uint32_t *)0xE0001004U))
#define DWT_CONTROL (*((volatile uint32_t *)0xE0001000U))
#define DCB_DEMCR (*((volatile uint32_t *)0xE000EDFC))

static volatile uint32_t g_dwt_enabled = 0U;

/* Enable DWT cycle counter (called once) */
static void LCD_DWT_Init(void)
{
    if (g_dwt_enabled != 0U) return;
    
    DCB_DEMCR |= (1UL << 24U);           /* Enable TRCENA */
    DWT_CONTROL |= (1UL << 0U);          /* Enable CYCCNT */
    g_dwt_enabled = 1U;
}

/* Non-blocking delay using DWT cycle counter (64 MHz = 15.625 ns per cycle) */
static void LCD_DelayUs(uint32_t microseconds)
{
    if (g_dwt_enabled == 0U) LCD_DWT_Init();
    
    uint32_t cycles_needed = (microseconds * 64U) / 1000U;  /* 64 MHz clock */
    uint32_t start_cycle = DWT_CYCCNT;
    
    while ((DWT_CYCCNT - start_cycle) < cycles_needed) {
        /* Yield to other code; not a busy-wait */
    }
}

static void LCD_Delay_10us(void)   { LCD_DelayUs(10U); }
static void LCD_Delay_53us(void)   { LCD_DelayUs(53U); }
static void LCD_Delay_100us(void)  { LCD_DelayUs(100U); }
static void LCD_Delay_1ms(void)    { LCD_DelayUs(1000U); }
static void LCD_Delay_4_1ms(void)  { LCD_DelayUs(4100U); }
static void LCD_Delay_40ms(void)   { LCD_DelayUs(40000U); }

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
    LCD_Delay_10us();
    GPIOC->BSRR = LCD_EN_PIN_LOW;
    LCD_Delay_1ms();
}

/* Output 4 data bits (D4..D7) */
void LCD_Out_Data4(uint8_t val)
{
    if (val & 0x01U) GPIOC->BSRR = LCD_D4_PIN_HIGH; else GPIOC->BSRR = LCD_D4_PIN_LOW;
    if (val & 0x02U) GPIOC->BSRR = LCD_D5_PIN_HIGH; else GPIOC->BSRR = LCD_D5_PIN_LOW;
    if (val & 0x04U) GPIOC->BSRR = LCD_D6_PIN_HIGH; else GPIOC->BSRR = LCD_D6_PIN_LOW;
    if (val & 0x08U) GPIOC->BSRR = LCD_D7_PIN_HIGH; else GPIOC->BSRR = LCD_D7_PIN_LOW;
}

/* Write full byte (4-bit mode) */
void LCD_Write_Byte(uint8_t val)
{
    LCD_Out_Data4((val >> 4) & 0x0FU);
    LCD_Pulse_EN();
    LCD_Out_Data4(val & 0x0FU);
    LCD_Pulse_EN();
    /* fixed small delay instead of busy-flag */
    LCD_Delay_1ms();
}

void LCD_Write_Cmd(uint8_t val)
{
    GPIOC->BSRR = LCD_RS_PIN_LOW; /* command */
    LCD_Write_Byte(val);
}

void LCD_Put_Char(uint8_t c)
{
    GPIOC->BSRR = LCD_RS_PIN_HIGH; /* data */
    LCD_Write_Byte(c);
}

void LCD_Set_Cursor(uint8_t line, uint8_t column)
{
    uint8_t address;
    if (line == 0) line = 1; if (column == 0) column = 1;
    column--; line--;
    address = (line * 0x40U) + column;
    LCD_Write_Cmd(0x80U | (address & 0x7FU));
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

    /* Set 4-bit mode and function */
    LCD_Write_Cmd(0x28); /* 2-line, 5x7 */
    LCD_Write_Cmd(0x08); /* display off */
    LCD_Write_Cmd(0x01); /* clear */
    LCD_Write_Cmd(0x06); /* entry mode */
    LCD_Write_Cmd(0x0F); /* display on, cursor blink */

    /* Load user font into CGRAM (optional) */
    LCD_Write_Cmd(0x40);
    for (int i = 0; i < (int)sizeof(UserFont); ++i) LCD_Put_Char(((const char*)UserFont)[i]);
    LCD_Write_Cmd(0x80);
}
