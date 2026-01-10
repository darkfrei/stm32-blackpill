# BlackPill STM32F411CE LED Blink

Simple LED blink project for WeAct STM32F411CEU6 BlackPill development board.

## Hardware

- **Board**: WeAct STM32F411CEU6 BlackPill
- **MCU**: STM32F411CEU6 (Cortex-M4, 100MHz, 512KB Flash, 128KB RAM)
- **Programmer**: ST-Link V2
- **LED Pin**: PA5 (can be changed in CubeMX configuration)

## Project Description

This is a minimal example that toggles an LED connected to pin PA5 every 500 milliseconds. The project demonstrates:

- Basic GPIO configuration
- HAL library usage
- System clock configuration at 100MHz using external 25MHz crystal
- ST-Link V2 programming and debugging setup

## Hardware Connections

### ST-Link V2 to BlackPill

```
ST-Link V2          BlackPill F411
----------          --------------
SWDIO       ----->  DIO (SWDIO)
SWCLK       ----->  DCLK (SWCLK)
GND         ----->  GND
3.3V        ----->  3V3 (optional, for power)
```

### LED Connection

Connect an external LED with current limiting resistor (220-330 ohm) to pin PA5, or use the onboard LED if available on your board version.

```
PA5 ---[LED]---[Resistor 220R]--- GND
```

## Software Requirements

### Required Tools

1. **STM32CubeMX** 6.12.0 or later
   - Download: https://www.st.com/en/development-tools/stm32cubemx.html

2. **STM32CubeIDE** 2.0.0 or later
   - Download: https://www.st.com/en/development-tools/stm32cubeide.html

3. **ST-Link Driver**
   - Download: https://www.st.com/en/development-tools/stsw-link009.html

### Optional Tools

- **STM32CubeProgrammer** - for standalone programming without IDE
- **Git** - for version control

## Project Setup

### Clone or Download

```bash
git clone <repository-url>
cd 001-blink
```

### Open in STM32CubeIDE

1. Launch STM32CubeIDE
2. File -> Open Projects from File System
3. Select the project folder containing `.project` file
4. Click Finish

### Modify Configuration (Optional)

To change pin assignments or clock settings:

1. Double-click on `001-blink.ioc` file
2. Modify settings in Device Configuration Tool
3. Press Ctrl+S to regenerate code

**Important**: Only modify code between `USER CODE BEGIN` and `USER CODE END` markers to preserve changes during code regeneration.

## Build and Flash

### Using STM32CubeIDE

1. Connect ST-Link V2 to BlackPill board
2. Connect ST-Link V2 to PC via USB
3. In STM32CubeIDE:
   - Build: Press `Ctrl+B` or click hammer icon
   - Flash and Debug: Press `F11` or click bug icon
   - Run without debug: Press `Ctrl+F11`

### Build Configurations

- **Debug**: Full debug symbols, no optimization (default)
- **Release**: Optimized for size, no debug symbols

Switch configuration: Right-click project -> Build Configurations -> Set Active

### Expected Result

After successful programming, the LED on PA5 should blink with 500ms on and 500ms off pattern.

## Code Structure

```
001-blink/
├── Core/
│   ├── Inc/                  # Header files
│   │   ├── main.h
│   │   ├── stm32f4xx_hal_conf.h
│   │   └── stm32f4xx_it.h
│   └── Src/                  # Source files
│       ├── main.c            # Main application code
│       ├── stm32f4xx_hal_msp.c
│       ├── stm32f4xx_it.c
│       └── system_stm32f4xx.c
├── Drivers/                  # HAL and CMSIS drivers
│   ├── STM32F4xx_HAL_Driver/
│   └── CMSIS/
├── 001-blink.ioc            # STM32CubeMX configuration
├── STM32F411CEUX_FLASH.ld   # Linker script
└── Makefile                 # Build system
```

## Main Code Snippet

```c
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  // turn off led initially
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  while (1)
  {
    // toggle led state
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    // wait 500 milliseconds
    HAL_Delay(500);
  }
}
```

## Clock Configuration

- **External Crystal**: 25MHz (HSE)
- **PLL Configuration**: Optimized for 100MHz SYSCLK
- **AHB Clock (HCLK)**: 100MHz
- **APB1 Clock**: 50MHz
- **APB2 Clock**: 100MHz

## Troubleshooting

### ST-Link not detected

- Install ST-Link drivers from ST website
- Try different USB port
- Check Device Manager for "STMicroelectronics STLink dongle"

### Cannot connect to target

- Verify physical connections (SWDIO, SWCLK, GND)
- Ensure BlackPill is powered (LED on board should light up)
- Try "Connect under reset" mode in Debug Configuration
- Lower SWD frequency to 1000kHz in Debug Configuration

### Build errors

- Clean project: Project -> Clean
- Rebuild: Ctrl+B
- Check that STM32CubeF4 firmware package is installed

### LED not blinking

- Verify LED polarity (anode to PA5, cathode to GND through resistor)
- Check GPIO configuration in .ioc file
- Verify code was successfully flashed (check Console output)
- Use debugger to step through code

## License

This project uses STMicroelectronics HAL library which is licensed under BSD-3-Clause license.

## Resources

- [STM32F411CE Datasheet](https://www.st.com/resource/en/datasheet/stm32f411ce.pdf)
- [STM32F411 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00119316-stm32f411xc-e-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)
- [BlackPill Schematic](https://github.com/WeActTC/MiniSTM32F4x1)
- [STM32 HAL Documentation](https://www.st.com/resource/en/user_manual/dm00105879-description-of-stm32f4-hal-and-ll-drivers-stmicroelectronics.pdf)

## Author

darkfrei

## Version History

- v1.0.0 (2025-01-10) - Initial release with basic LED blink functionality
