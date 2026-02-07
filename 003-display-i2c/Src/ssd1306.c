
// file ssd1306.c
#include "ssd1306.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>  // for memcpy

#if defined(SSD1306_USE_I2C)

void ssd1306_Reset(void) {
    /* for i2c - do nothing */
}

// send a byte to the command register
void ssd1306_WriteCommand(uint8_t byte) {
    HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &byte, 1, HAL_MAX_DELAY);
}

// send data
void ssd1306_WriteData(uint8_t* buffer, size_t buff_size) {
    HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x40, 1, buffer, buff_size, HAL_MAX_DELAY);
}

#ifdef SSD1306_USE_DMA
void ssd1306_WriteData_DMA(uint8_t* buffer, size_t buff_size) {
    static volatile uint8_t dma_busy = 0;
    
    // wait for previous transfer to complete
    while(dma_busy) {
        __NOP();
    }
    
    dma_busy = 1;
    if (HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x40, 1, buffer, buff_size) != HAL_OK) {
        dma_busy = 0;
        // fall back to blocking write
        ssd1306_WriteData(buffer, buff_size);
    }
}

// i2c dma transfer complete callback (user must implement in main)
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == SSD1306_I2C_PORT.Instance) {
        // signal that dma is free
        // note: user should implement this in their main.c
    }
}
#endif

#elif defined(SSD1306_USE_SPI)

void ssd1306_Reset(void) {
    // cs = high (not selected)
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET);

    // reset the oled
    HAL_GPIO_WritePin(SSD1306_Reset_Port, SSD1306_Reset_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(SSD1306_Reset_Port, SSD1306_Reset_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
}

// send a byte to the command register
void ssd1306_WriteCommand(uint8_t byte) {
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_RESET); // select oled
    HAL_GPIO_WritePin(SSD1306_DC_Port, SSD1306_DC_Pin, GPIO_PIN_RESET); // command
    HAL_SPI_Transmit(&SSD1306_SPI_PORT, (uint8_t *) &byte, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET); // un-select oled
}

// send data
void ssd1306_WriteData(uint8_t* buffer, size_t buff_size) {
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_RESET); // select oled
    HAL_GPIO_WritePin(SSD1306_DC_Port, SSD1306_DC_Pin, GPIO_PIN_SET); // data
    HAL_SPI_Transmit(&SSD1306_SPI_PORT, buffer, buff_size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET); // un-select oled
}

#else
#error "you should define SSD1306_USE_SPI or SSD1306_USE_I2C macro"
#endif


// screenbuffer
#ifdef SSD1306_DOUBLE_BUFFER
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];
static uint8_t SSD1306_BackBuffer[SSD1306_BUFFER_SIZE];
#else
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];
#endif

// screen object
static SSD1306_t SSD1306;

/* external state for incremental updater */
volatile uint16_t ssd1306UpdatePosition = 0; /* byte offset in buffer */
volatile uint8_t  ssd1306UpdatePage     = 0; /* current page index */
volatile uint8_t  ssd1306CursorX        = 0; /* x position inside page */
volatile uint8_t  ssd1306DirtyFlag      = 0; /* dirty region flag */

/* number of bytes to send in one update call */
#ifndef SSD1306_UPDATE_CHUNK_SIZE
#define SSD1306_UPDATE_CHUNK_SIZE 32U
#endif

/* helper function to mark region as dirty */
static void ssd1306_MarkDirty(uint16_t start, uint16_t end) {
    if (start > end) {
        uint16_t temp = start;
        start = end;
        end = temp;
    }
    
    if (SSD1306.Dirty) {
        if (start < SSD1306.DirtyStart) SSD1306.DirtyStart = start;
        if (end > SSD1306.DirtyEnd) SSD1306.DirtyEnd = end;
    } else {
        SSD1306.Dirty = 1;
        SSD1306.DirtyStart = start;
        SSD1306.DirtyEnd = end;
    }
    
    ssd1306DirtyFlag = 1;
}

/* fills the screenbuffer with values from a given buffer of a fixed length */
SSD1306_Error_t ssd1306_FillBuffer(uint8_t* buf, uint32_t len) {
    SSD1306_Error_t ret = SSD1306_ERR;
    if (len <= SSD1306_BUFFER_SIZE) {
        memcpy(SSD1306_Buffer, buf, len);
        ssd1306_MarkDirty(0, len - 1);
        ret = SSD1306_OK;
    }
    return ret;
}

/* initialize the oled screen */
SSD1306_Error_t ssd1306_Init(void) {
    // reset oled
    ssd1306_Reset();

    // wait for the screen to boot
    HAL_Delay(100);

    // check i2c communication
    #ifdef SSD1306_USE_I2C
        if (HAL_I2C_IsDeviceReady(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 3, 100) != HAL_OK) {
            return SSD1306_ERR;
        }
    #endif

    // init oled
    ssd1306_SetDisplayOn(0); // display off

    ssd1306_WriteCommand(0x20); // set memory addressing mode
    ssd1306_WriteCommand(0x00); // 00b, horizontal addressing mode

    ssd1306_WriteCommand(0xB0); // set page start address for page addressing mode, 0-7

#ifdef SSD1306_MIRROR_VERT
    ssd1306_WriteCommand(0xC0); // mirror vertically
#else
    ssd1306_WriteCommand(0xC8); // set com output scan direction
#endif

    ssd1306_WriteCommand(0x00); // set low column address
    ssd1306_WriteCommand(0x10); // set high column address

    ssd1306_WriteCommand(0x40); // set start line address

    ssd1306_SetContrast(0xFF);

#ifdef SSD1306_MIRROR_HORIZ
    ssd1306_WriteCommand(0xA0); // mirror horizontally
#else
    ssd1306_WriteCommand(0xA1); // set segment re-map 0 to 127
#endif

#ifdef SSD1306_INVERSE_COLOR
    ssd1306_WriteCommand(0xA7); // set inverse color
#else
    ssd1306_WriteCommand(0xA6); // set normal color
#endif

/* set multiplex ratio (command 0xA8 then value = SSD1306_HEIGHT - 1) */
ssd1306_WriteCommand(0xA8);
ssd1306_WriteCommand((uint8_t)(SSD1306_HEIGHT - 1));
/* optionally keep compile-time check that SSD1306_HEIGHT is supported */
#if !(SSD1306_HEIGHT == 32 || SSD1306_HEIGHT == 64 || SSD1306_HEIGHT == 128)
#error "only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_WriteCommand(0xA4); // 0xa4, output follows ram content

    ssd1306_WriteCommand(0xD3); // set display offset
    ssd1306_WriteCommand(0x00); // not offset

    ssd1306_WriteCommand(0xD5); // set display clock divide ratio/oscillator frequency
    ssd1306_WriteCommand(0xF0); // set divide ratio

    ssd1306_WriteCommand(0xD9); // set pre-charge period
    ssd1306_WriteCommand(0x22);

    ssd1306_WriteCommand(0xDA); // set com pins hardware configuration
#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(0x02);
#elif (SSD1306_HEIGHT == 64)
    ssd1306_WriteCommand(0x12);
#elif (SSD1306_HEIGHT == 128)
    ssd1306_WriteCommand(0x12);
#else
#error "only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_WriteCommand(0xDB); // set vcomh
    ssd1306_WriteCommand(0x20); // 0x20, 0.77xVcc

    ssd1306_WriteCommand(0x8D); // set dc-dc enable
    ssd1306_WriteCommand(0x14);
    ssd1306_SetDisplayOn(1); // turn on SSD1306 panel

    // clear screen
    ssd1306_Fill(Black);
    
    // flush buffer to screen
    ssd1306_UpdateScreen();
    
    // set default values for screen object
    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;
    SSD1306.Initialized = 1;
    SSD1306.Dirty = 0;
    SSD1306.DirtyStart = 0xFFFF;
    SSD1306.DirtyEnd = 0;
    
    // reset update position
    ssd1306UpdatePosition = 0;
    ssd1306UpdatePage = 0;
    ssd1306CursorX = 0;
    ssd1306DirtyFlag = 0;
    
    return SSD1306_OK;
}

/* fill the whole screen with the given color */
void ssd1306_Fill(SSD1306_COLOR color) {
    memset(SSD1306_Buffer, (color == Black) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
    ssd1306_MarkDirty(0, SSD1306_BUFFER_SIZE - 1);
}

/* write the screenbuffer with changed to the screen */
void ssd1306_UpdateScreen(void) {
    // write data to each page of ram. number of pages
    // depends on the screen height:
    //
    //  * 32px   ==  4 pages
    //  * 64px   ==  8 pages
    //  * 128px  ==  16 pages
    for(uint8_t i = 0; i < SSD1306_HEIGHT/8; i++) {
        ssd1306_WriteCommand(0xB0 + i); // set the current ram page address.
        ssd1306_WriteCommand(0x00 + SSD1306_X_OFFSET_LOWER);
        ssd1306_WriteCommand(0x10 + SSD1306_X_OFFSET_UPPER);
        
        ssd1306_WriteData(&SSD1306_Buffer[SSD1306_WIDTH*i], SSD1306_WIDTH);
    }
}

/*
 * draw one pixel in the screenbuffer
 * x => x coordinate
 * y => y coordinate
 * color => pixel color
 */
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color) {
    if(x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        // don't write outside the buffer
        return;
    }
    
    uint16_t pos = x + (y / 8) * SSD1306_WIDTH;
    
    // mark region as dirty
    ssd1306_MarkDirty(pos, pos);
    
    // draw in the right color
    if(color == White) {
        SSD1306_Buffer[pos] |= 1 << (y % 8);
    } else { 
        SSD1306_Buffer[pos] &= ~(1 << (y % 8));
    }
}

/*
 * draw 1 char to the screen buffer
 * ch       => char to write
 * font     => font to use
 * color    => black or white
 */
char ssd1306_WriteChar(char ch, SSD1306_Font_t Font, SSD1306_COLOR color) {
    uint32_t i, b, j;
    
    // check if character is valid
    if (ch < 32 || ch > 126)
        return 0;
    
    // char width is not equal to font width for proportional font
    const uint8_t char_width = Font.char_width ? Font.char_width[ch-32] : Font.width;
    // check remaining space on current line
    if (SSD1306_WIDTH < (SSD1306.CurrentX + char_width) ||
        SSD1306_HEIGHT < (SSD1306.CurrentY + Font.height))
    {
        // not enough space on current line
        return 0;
    }
    
    // mark region as dirty
    uint16_t start_pos = SSD1306.CurrentX + (SSD1306.CurrentY / 8) * SSD1306_WIDTH;
    uint16_t end_pos = SSD1306.CurrentX + char_width + ((SSD1306.CurrentY + Font.height) / 8) * SSD1306_WIDTH;
    ssd1306_MarkDirty(start_pos, end_pos);
    
    // use the font to write
    for(i = 0; i < Font.height; i++) {
        b = Font.data[(ch - 32) * Font.height + i];
        for(j = 0; j < char_width; j++) {
            if((b << j) & 0x8000)  {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR) color);
            } else {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR)!color);
            }
        }
    }
    
    // the current space is now taken
    SSD1306.CurrentX += char_width;
    
    // return written char for validation
    return ch;
}

/* write full string to screenbuffer */
char ssd1306_WriteString(char* str, SSD1306_Font_t Font, SSD1306_COLOR color) {
    while (*str) {
        if (ssd1306_WriteChar(*str, Font, color) != *str) {
            // char could not be written
            return *str;
        }
        str++;
    }
    
    // everything ok
    return *str;
}

/* position the cursor */
void ssd1306_SetCursor(uint8_t x, uint8_t y) {
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}

/* draw line by bresenham's algorithm */
void ssd1306_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    int32_t deltaX = abs(x2 - x1);
    int32_t deltaY = abs(y2 - y1);
    int32_t signX = ((x1 < x2) ? 1 : -1);
    int32_t signY = ((y1 < y2) ? 1 : -1);
    int32_t error = deltaX - deltaY;
    int32_t error2;
    
    ssd1306_DrawPixel(x2, y2, color);

    while((x1 != x2) || (y1 != y2)) {
        ssd1306_DrawPixel(x1, y1, color);
        error2 = error * 2;
        if(error2 > -deltaY) {
            error -= deltaY;
            x1 += signX;
        }
        
        if(error2 < deltaX) {
            error += deltaX;
            y1 += signY;
        }
    }
    return;
}

/* draw polyline */
void ssd1306_Polyline(const SSD1306_VERTEX *par_vertex, uint16_t par_size, SSD1306_COLOR color) {
    uint16_t i;
    if(par_vertex == NULL) {
        return;
    }

    for(i = 1; i < par_size; i++) {
        ssd1306_Line(par_vertex[i - 1].x, par_vertex[i - 1].y, par_vertex[i].x, par_vertex[i].y, color);
    }

    return;
}

/* convert degrees to radians */
static float ssd1306_DegToRad(float par_deg) {
    return par_deg * (3.14159f / 180.0f);
}

/* normalize degree to [0;360] */
static uint16_t ssd1306_NormalizeTo0_360(uint16_t par_deg) {
    uint16_t loc_angle;
    if(par_deg <= 360) {
        loc_angle = par_deg;
    } else {
        loc_angle = par_deg % 360;
        loc_angle = (loc_angle ? loc_angle : 360);
    }
    return loc_angle;
}

/*
 * drawarc. draw angle is beginning from 4 quart of trigonometric circle (3pi/2)
 * start_angle in degree
 * sweep in degree
 */
void ssd1306_DrawArc(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color) {
    static const uint8_t CIRCLE_APPROXIMATION_SEGMENTS = 36;
    float approx_degree;
    uint32_t approx_segments;
    uint8_t xp1,xp2;
    uint8_t yp1,yp2;
    uint32_t count;
    uint32_t loc_sweep;
    float rad;
    
    loc_sweep = ssd1306_NormalizeTo0_360(sweep);
    
    count = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree = loc_sweep / (float)approx_segments;
    while(count < approx_segments)
    {
        rad = ssd1306_DegToRad(count*approx_degree);
        xp1 = x + (int8_t)(sinf(rad)*radius);
        yp1 = y + (int8_t)(cosf(rad)*radius);    
        count++;
        if(count != approx_segments) {
            rad = ssd1306_DegToRad(count*approx_degree);
        } else {
            rad = ssd1306_DegToRad(loc_sweep);
        }
        xp2 = x + (int8_t)(sinf(rad)*radius);
        yp2 = y + (int8_t)(cosf(rad)*radius);    
        ssd1306_Line(xp1,yp1,xp2,yp2,color);
    }
    
    return;
}

/*
 * draw arc with radius line
 * angle is beginning from 4 quart of trigonometric circle (3pi/2)
 * start_angle: start angle in degree
 * sweep: finish angle in degree
 */
void ssd1306_DrawArcWithRadiusLine(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color) {
    const uint32_t CIRCLE_APPROXIMATION_SEGMENTS = 36;
    float approx_degree;
    uint32_t approx_segments;
    uint8_t xp1;
    uint8_t xp2 = 0;
    uint8_t yp1;
    uint8_t yp2 = 0;
    uint32_t count;
    uint32_t loc_sweep;
    float rad;
    
    loc_sweep = ssd1306_NormalizeTo0_360(sweep);
    
    count = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree = loc_sweep / (float)approx_segments;

    rad = ssd1306_DegToRad(count*approx_degree);
    uint8_t first_point_x = x + (int8_t)(sinf(rad)*radius);
    uint8_t first_point_y = y + (int8_t)(cosf(rad)*radius);   
    while (count < approx_segments) {
        rad = ssd1306_DegToRad(count*approx_degree);
        xp1 = x + (int8_t)(sinf(rad)*radius);
        yp1 = y + (int8_t)(cosf(rad)*radius);    
        count++;
        if (count != approx_segments) {
            rad = ssd1306_DegToRad(count*approx_degree);
        } else {
            rad = ssd1306_DegToRad(loc_sweep);
        }
        xp2 = x + (int8_t)(sinf(rad)*radius);
        yp2 = y + (int8_t)(cosf(rad)*radius);    
        ssd1306_Line(xp1,yp1,xp2,yp2,color);
    }
    
    // radius line
    ssd1306_Line(x,y,first_point_x,first_point_y,color);
    ssd1306_Line(x,y,xp2,yp2,color);
    return;
}

/* draw circle by bresenham's algorithm */
void ssd1306_DrawCircle(uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color) {
    int32_t x = -par_r;
    int32_t y = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) {
        return;
    }

    do {
        ssd1306_DrawPixel(par_x - x, par_y + y, par_color);
        ssd1306_DrawPixel(par_x + x, par_y + y, par_color);
        ssd1306_DrawPixel(par_x + x, par_y - y, par_color);
        ssd1306_DrawPixel(par_x - x, par_y - y, par_color);
        e2 = err;

        if (e2 <= y) {
            y++;
            err = err + (y * 2 + 1);
            if(-x == y && e2 <= x) {
                e2 = 0;
            }
        }

        if (e2 > x) {
            x++;
            err = err + (x * 2 + 1);
        }
    } while (x <= 0);

    return;
}

/* draw filled circle. pixel positions calculated using bresenham's algorithm */
void ssd1306_FillCircle(uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color) {
    int32_t x = -par_r;
    int32_t y = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) {
        return;
    }

    do {
        int16_t y0 = par_y - y;
        int16_t y1 = par_y + y;
        int16_t x0 = par_x - x;
        int16_t x1 = par_x + x;

        for (int16_t yy = y0; yy <= y1; yy++) {
            if (yy < 0 || yy >= SSD1306_HEIGHT) continue;
            for (int16_t xx = x0; xx <= x1; xx++) {
                if (xx < 0 || xx >= SSD1306_WIDTH) continue;
                ssd1306_DrawPixel((uint8_t)xx, (uint8_t)yy, par_color);
            }
        }

        e2 = err;
        if (e2 <= y) {
            y++;
            err = err + (y * 2 + 1);
            if (-x == y && e2 <= x) {
                e2 = 0;
            }
        }

        if (e2 > x) {
            x++;
            err = err + (x * 2 + 1);
        }
    } while (x <= 0);
}


/* draw a rectangle */
void ssd1306_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    ssd1306_Line(x1,y1,x2,y1,color);
    ssd1306_Line(x2,y1,x2,y2,color);
    ssd1306_Line(x2,y2,x1,y2,color);
    ssd1306_Line(x1,y2,x1,y1,color);

    return;
}

/* draw a filled rectangle */
void ssd1306_FillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    uint8_t x_start = ((x1<=x2) ? x1 : x2);
    uint8_t x_end   = ((x1<=x2) ? x2 : x1);
    uint8_t y_start = ((y1<=y2) ? y1 : y2);
    uint8_t y_end   = ((y1<=y2) ? y2 : y1);

    for (uint8_t y= y_start; (y<= y_end)&&(y<SSD1306_HEIGHT); y++) {
        for (uint8_t x= x_start; (x<= x_end)&&(x<SSD1306_WIDTH); x++) {
            ssd1306_DrawPixel(x, y, color);
        }
    }
    return;
}

SSD1306_Error_t ssd1306_InvertRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  if ((x2 >= SSD1306_WIDTH) || (y2 >= SSD1306_HEIGHT)) {
    return SSD1306_ERR;
  }
  if ((x1 > x2) || (y1 > y2)) {
    return SSD1306_ERR;
  }
  uint32_t i;
  
  // mark region as dirty
  uint16_t start_pos = x1 + (y1 / 8) * SSD1306_WIDTH;
  uint16_t end_pos = x2 + (y2 / 8) * SSD1306_WIDTH;
  ssd1306_MarkDirty(start_pos, end_pos);
  
  if ((y1 / 8) != (y2 / 8)) {
    /* if rectangle doesn't lie on one 8px row */
    for (uint32_t x = x1; x <= x2; x++) {
      i = x + (y1 / 8) * SSD1306_WIDTH;
      SSD1306_Buffer[i] ^= 0xFF << (y1 % 8);
      i += SSD1306_WIDTH;
      for (; i < x + (y2 / 8) * SSD1306_WIDTH; i += SSD1306_WIDTH) {
        SSD1306_Buffer[i] ^= 0xFF;
      }
      SSD1306_Buffer[i] ^= 0xFF >> (7 - (y2 % 8));
    }
  } else {
    /* if rectangle lies on one 8px row */
    const uint8_t mask = (0xFF << (y1 % 8)) & (0xFF >> (7 - (y2 % 8)));
    for (i = x1 + (y1 / 8) * SSD1306_WIDTH;
         i <= (uint32_t)x2 + (y2 / 8) * SSD1306_WIDTH; i++) {
      SSD1306_Buffer[i] ^= mask;
    }
  }
  return SSD1306_OK;
}

/* draw a bitmap */
void ssd1306_DrawBitmap(uint8_t x, uint8_t y, const unsigned char* bitmap, uint8_t w, uint8_t h, SSD1306_COLOR color) {
    int16_t byteWidth = (w + 7) / 8; // bitmap scanline pad = whole byte
    uint8_t byte = 0;

    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;
    }
    
    // mark region as dirty
    uint16_t start_pos = x + (y / 8) * SSD1306_WIDTH;
    uint16_t end_pos = x + w + ((y + h) / 8) * SSD1306_WIDTH;
    ssd1306_MarkDirty(start_pos, end_pos);

    for (uint8_t j = 0; j < h; j++, y++) {
        for (uint8_t i = 0; i < w; i++) {
            if (i & 7) {
                byte <<= 1;
            } else {
                byte = (*(const unsigned char *)(&bitmap[j * byteWidth + i / 8]));
            }

            if (byte & 0x80) {
                ssd1306_DrawPixel(x + i, y, color);
            }
        }
    }
    return;
}

void ssd1306_SetContrast(const uint8_t value) {
    const uint8_t kSetContrastControlRegister = 0x81;
    ssd1306_WriteCommand(kSetContrastControlRegister);
    ssd1306_WriteCommand(value);
}

void ssd1306_SetDisplayOn(const uint8_t on) {
    uint8_t value;
    if (on) {
        value = 0xAF;   // display on
        SSD1306.DisplayOn = 1;
    } else {
        value = 0xAE;   // display off
        SSD1306.DisplayOn = 0;
    }
    ssd1306_WriteCommand(value);
}

uint8_t ssd1306_GetDisplayOn() {
    return SSD1306.DisplayOn;
}

/* -------------------------------------------------------------- */
/* -------------------------------------------------------------- */
/* -------------------------------------------------------------- */

/* ensure x offset macro exists */
#ifndef SSD1306_X_OFFSET
#define SSD1306_X_OFFSET 0
#endif

void ssd1306_UpdateScreenChunk(void)
{
    static uint8_t last_page = 0xFF;
    static uint16_t last_col = 0xFFFF;
    
    const uint16_t totalSize = (uint16_t)(SSD1306_WIDTH * (SSD1306_HEIGHT / 8U));
    uint16_t bytesToSend;
    uint8_t page;
    uint16_t col;
    uint16_t remainingInPage;

    /* normalize position if out of bounds */
    if (ssd1306UpdatePosition >= totalSize) {
        ssd1306UpdatePosition = 0;
        ssd1306UpdatePage = 0;
        ssd1306CursorX = 0;
    }

    /* calculate current page and x position from linear byte offset */
    page = (uint8_t)(ssd1306UpdatePosition / SSD1306_WIDTH);
    ssd1306CursorX = (uint8_t)(ssd1306UpdatePosition % SSD1306_WIDTH);
    ssd1306UpdatePage = page;

    /* compute column start = x + X_OFFSET */
    col = (uint16_t)ssd1306CursorX + (uint16_t)SSD1306_X_OFFSET;

    /* only send page/column commands if they changed */
    if (last_page != page || last_col != col) {
        ssd1306_WriteCommand((uint8_t)(0xB0 + page));
        ssd1306_WriteCommand((uint8_t)(0x00 + (col & 0x0F)));
        ssd1306_WriteCommand((uint8_t)(0x10 + ((col >> 4) & 0x0F)));
        
        last_page = page;
        last_col = col;
    }

    /* limit transfer to remaining bytes in the current page row */
    remainingInPage = (uint16_t)SSD1306_WIDTH - (uint16_t)ssd1306CursorX;
    bytesToSend = (uint16_t)SSD1306_UPDATE_CHUNK_SIZE;
    
    if (bytesToSend > remainingInPage) {
        bytesToSend = remainingInPage;
    }
    if ((uint32_t)ssd1306UpdatePosition + bytesToSend > totalSize) {
        bytesToSend = (uint16_t)(totalSize - ssd1306UpdatePosition);
    }

    /* send the data chunk (single page boundary guaranteed) */
    if (bytesToSend > 0) {
#ifdef SSD1306_USE_DMA
        ssd1306_WriteData_DMA(&SSD1306_Buffer[ssd1306UpdatePosition], bytesToSend);
#else
        ssd1306_WriteData(&SSD1306_Buffer[ssd1306UpdatePosition], bytesToSend);
#endif
        ssd1306UpdatePosition = (uint16_t)(ssd1306UpdatePosition + bytesToSend);
        ssd1306CursorX += bytesToSend;
    }

    /* move to next page if we reached the end of current page */
    if (ssd1306CursorX >= SSD1306_WIDTH) {
        ssd1306CursorX = 0;
        ssd1306UpdatePage++;
        last_col = 0xFFFF; // force column command on next page
    }

    /* reset after full frame */
    if (ssd1306UpdatePosition >= totalSize) {
        ssd1306UpdatePosition = 0;
        ssd1306UpdatePage = 0;
        ssd1306CursorX = 0;
        last_page = 0xFF;
        last_col = 0xFFFF;
    }
}

void ssd1306_UpdateDirtyChunk(void)
{
    static uint16_t dirty_update_pos = 0;
    
    if (!SSD1306.Dirty) {
        dirty_update_pos = 0;
        ssd1306DirtyFlag = 0;
        return;
    }

    const uint16_t totalSize = (uint16_t)(SSD1306_WIDTH * (SSD1306_HEIGHT / 8U));
    uint16_t bytesToSend;
    uint8_t page;
    uint16_t col;

    /* start from beginning of dirty region if not already updating */
    if (dirty_update_pos < SSD1306.DirtyStart) {
        dirty_update_pos = SSD1306.DirtyStart;
    }

    /* check if we've passed the dirty region */
    if (dirty_update_pos > SSD1306.DirtyEnd) {
        SSD1306.Dirty = 0;
        SSD1306.DirtyStart = 0xFFFF;
        SSD1306.DirtyEnd = 0;
        dirty_update_pos = 0;
        ssd1306DirtyFlag = 0;
        return;
    }

    /* calculate current page and column */
    page = (uint8_t)(dirty_update_pos / SSD1306_WIDTH);
    col = (uint8_t)(dirty_update_pos % SSD1306_WIDTH) + SSD1306_X_OFFSET;

    /* set display position */
    ssd1306_WriteCommand((uint8_t)(0xB0 + page));
    ssd1306_WriteCommand((uint8_t)(0x00 + (col & 0x0F)));
    ssd1306_WriteCommand((uint8_t)(0x10 + ((col >> 4) & 0x0F)));

    /* calculate bytes to send (respect chunk size and dirty region end) */
    bytesToSend = SSD1306_UPDATE_CHUNK_SIZE;
    if (dirty_update_pos + bytesToSend > SSD1306.DirtyEnd + 1) {
        bytesToSend = SSD1306.DirtyEnd + 1 - dirty_update_pos;
    }

    /* don't cross page boundary */
    uint16_t remainingInPage = SSD1306_WIDTH - (dirty_update_pos % SSD1306_WIDTH);
    if (bytesToSend > remainingInPage) {
        bytesToSend = remainingInPage;
    }

    /* send data */
    if (bytesToSend > 0) {
#ifdef SSD1306_USE_DMA
        ssd1306_WriteData_DMA(&SSD1306_Buffer[dirty_update_pos], bytesToSend);
#else
        ssd1306_WriteData(&SSD1306_Buffer[dirty_update_pos], bytesToSend);
#endif
        dirty_update_pos += bytesToSend;
    }

    /* if we reached end of page, move to next page start */
    if (dirty_update_pos % SSD1306_WIDTH == 0) {
        dirty_update_pos = (dirty_update_pos / SSD1306_WIDTH) * SSD1306_WIDTH;
    }
}
