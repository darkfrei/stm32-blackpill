# EN11 Encoder on STM32F411 BlackPill with I2C Display

## Part 1 — STM32CubeMX Setup

### 1.1 Create a Project
1. Start STM32CubeMX
2. File → New Project
3. Select STM32F411CEU6 (BlackPill)
4. Click Start Project

### 1.2 Clock Configuration (RCC)
**Option 1 — External crystal (HSE):**
- Assign PH0 → RCC_OSC_IN
- Assign PH1 → RCC_OSC_OUT
- Verify in Categories → System Core → RCC → GPIO Settings

**Option 2 — Internal oscillator (HSI, default):**
- No configuration needed

**System Clock:**
- HSE 25 MHz: PLLM=25, PLLN=200, PLLP=2 → HCLK=100MHz
- HSI 16 MHz: Enter HCLK=100MHz → CubeMX sets PLL automatically

### 1.3 Encoder Pins
- CLK (A) → PA0 → GPIO_EXTI0
- DT  (B) → PA1 → GPIO_EXTI1
- SW  → PA2 → GPIO_EXTI2

GPIO Settings:
- PA0/PA1: External Interrupt Mode, Rising/Falling edge, Pull-up
- PA2: External Interrupt Mode, Falling edge, Pull-up

### 1.4 NVIC Interrupts
- Enable EXTI0, EXTI1, EXTI2
- Preemption Priority = 0 (default)

### 1.5 I2C Display
- PB6 → I2C1_SCL
- PB7 → I2C1_SDA
- I2C Mode: Fast Mode (400 kHz)
- GPIO Pull-up/Pull-down: as needed

### 1.6 Generate Code
- Project Name: Encoder_EN11_Display
- Toolchain: STM32CubeIDE (recommended)
- Generate peripheral init files (.c/.h)
- Click GENERATE CODE → Open Project

## Part 2 — Firmware

### 2.1 SSD1306 Library
Create `ssd1306.h` and `ssd1306.c` or use an existing library.

```c
#ifndef SSD1306_H
#define SSD1306_H
#include "main.h"
#define SSD1306_I2C_ADDR 0x78
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
void SSD1306_Init(void);
void SSD1306_Clear(void);
void SSD1306_UpdateScreen(void);
void SSD1306_GotoXY(uint8_t x, uint8_t y);
void SSD1306_Puts(char* str, uint8_t size);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
#endif
```

### 2.2 Encoder Handling (main.c)
- Reads both pins on any interrupt
- Detects full 4-transition cycles
- Debounces button with 200ms window

```c
volatile int32_t encoder_value=0;
volatile uint8_t encoder_button_pressed=0;
volatile uint16_t encoder_state=0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
 if(GPIO_Pin==GPIO_PIN_0||GPIO_Pin==GPIO_PIN_1){
  uint8_t clk=HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0);
  uint8_t dt=HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_1);
  uint8_t current=(clk<<1)|dt;
  encoder_state=(encoder_state<<2)|current;
  uint8_t pattern=encoder_state&0xFF;
  if(pattern==0xE1) {encoder_value++;encoder_state=0;}
  else if(pattern==0xD2){encoder_value--;encoder_state=0;}
 }
 if(GPIO_Pin==GPIO_PIN_2){
  static uint32_t last_press=0;
  uint32_t now=HAL_GetTick();
  if((now-last_press)>200){
   if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_2)==GPIO_PIN_RESET){encoder_button_pressed=1;encoder_value=0;last_press=now;}
  }
 }
}
```

## Part 3 — Hardware Connections
**EN11 Encoder:**
```
CLK → PA0
DT  → PA1
SW  → PA2
+   → 3.3V
GND → GND
```
**SSD1306 I2C Display:**
```
VCC → 3.3V
GND → GND
SCL → PB6
SDA → PB7
```

## Part 4 — Notes and Fixes
- Read both pins on any interrupt to prevent missed steps
- Detect full 4-transition patterns: 0xE1 CW, 0xD2 CCW
- Debounce button using 200ms window
- Optional RC filters for encoder signals
- Fast Mode I2C recommended for display

## Testing
1. Flash firmware
2. Rotate encoder → value increases/decreases
3. Press button → resets value to 0
4. Verify real-time display updates

