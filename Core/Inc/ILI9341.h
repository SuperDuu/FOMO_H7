/*
 * ILI9341.h
 * ILI9341 SPI DMA Driver
 * Created on: Aug 5, 2024
 *     Author: K.Rudenko
 */

#ifndef ILI9341_H_
#define ILI9341_H_

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

#include "main.h"
#include "stm32h7xx.h"
/******************************************************************************
 *                               GLOBAL MACRO                                 *
 ******************************************************************************/

#define RGB888(r,g,b)                          (((r) << 16) | ((g) << 8) | (b))
#define RGB888_TO_RGB565(r, g, b)\
                             (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#define RGB888_SIZE_BYTES                                                  (3U)
#define RGB565_SIZE_BYTES                                                  (2U)

/******************************************************************************
 *                            CONFIGURATION MACRO                             *
 ******************************************************************************/
#define SRC_WIDTH   320
#define SRC_HEIGHT  240
#define DST_WIDTH   128
#define DST_HEIGHT  128
#define CROP_WIDTH  240
#define CROP_HEIGHT 240
#define FOMO_CROP_SIZE 240
#define FOMO_CONF_THRESHOLD 0.50f
//#define MAX_DETECTIONS 3
#define ILI9341_PORTRAIT                    0
#define ILI9341_LANDSCAPE                   1
#define ILI9341_ORIENTATION                 ILI9341_LANDSCAPE

#define ILI9341_WIDTH                       (240U)
#define ILI9341_HEIGHT                      (320U)

#if(ILI9341_ORIENTATION == ILI9341_PORTRAIT)
    #define  ILI9341_ACTIVE_WIDTH           ILI9341_WIDTH
    #define  ILI9341_ACTIVE_HEIGHT          ILI9341_HEIGHT
#elif(ILI9341_ORIENTATION == ILI9341_LANDSCAPE)
    #define  ILI9341_ACTIVE_WIDTH           ILI9341_HEIGHT
    #define  ILI9341_ACTIVE_HEIGHT          ILI9341_WIDTH
#endif



/* Delay API */
#define ILI9341_DELAY(ms)                   HAL_Delay(ms)

/* Set as 1 if SPI CS pin is managed by hardware */
#define ILI9341_SPI_CS_HW_MANAGE            0

/* GPIO configuration */
#define ILI9341_GPIO_PORT_RESX              LCD_RESX_GPIO_Port
#define ILI9341_GPIO_PIN_RESX               LCD_RESX_Pin

#if (ILI9341_SPI_CS_HW_MANAGE == 1)
#define ILI9341_GPIO_PORT_CSX
#define ILI9341_GPIO_PIN_CSX
#else
#define ILI9341_GPIO_PORT_CSX               LCD_CSX_GPIO_Port
#define ILI9341_GPIO_PIN_CSX                LCD_CSX_Pin
#endif

#define ILI9341_GPIO_PORT_DCX               LCD_DCX_GPIO_Port
#define ILI9341_GPIO_PIN_DCX                LCD_DCX_Pin

/* Example for ILI9341 <-> STM32F407VET6 connections:
 * DCX/DC     -> PD9  (GPIO)
 * RESX/RESET -> PD10 (GPIO)
 * LED        -> PA7  (TIM14 CH1 PWM)
 * CSX/CS     -> PB12 (SPI2 NSS)
 * SCL/SCK    -> PB13 (SPI2 SCK)
 * SDI        -> PB15 (SPI2 MOSI)
 * SDO        -> PC2  (SPI2 MISO)
 * GND        -> GND
 * VCC        -> 5V
 */
/******************************************************************************
 *                           GLOBAL DATA TYPES                                *
 ******************************************************************************/

/* Pixel format */
typedef enum
{
    ILI9341_PIXEL_FMT_L8     = 1U,
    ILI9341_PIXEL_FMT_RGB565 = 2U,
    ILI9341_PIXEL_FMT_RGB666 = 3U,
    ILI9341_PIXEL_FMT_RGB888 = 4U,
}ILI9341_PixFormat_t;

/* Basic colors definitions in RGB888 */
enum
{
    VIOLET = RGB888(148, 0, 211),
    INDIGO = RGB888(75, 0, 130),
    BLUE   = RGB888(0, 0, 255),
    GREEN  = RGB888(0, 255, 0),
    YELLOW = RGB888(255, 255, 0),
    ORANGE = RGB888(255, 127, 0),
    RED    = RGB888(255, 0, 0),
    WHITE  = RGB888(255, 255, 255),
    BLACK  = RGB888(0, 0, 0),
};

/* Callback enumeration */
typedef enum
{
    ILI9341_TC_CBK,
    ILI9341_ERR_CBK,
} ILI9341_CB_t;

/* Callback pointer type */
typedef void (*ILI9341_FncPtr_t)(void);

/******************************************************************************
 *                      GLOBAL FUNCTIONS PROTOTYPES                           *
 ******************************************************************************/

extern void ILI9341_Init(SPI_HandleTypeDef* spi_handle, ILI9341_PixFormat_t pixFormat);
extern void ILI9341_RegisterCallback(ILI9341_CB_t cb_type, ILI9341_FncPtr_t fnc_ptr);
extern void ILI9341_SetBackgroundColor(uint32_t rgb888);
extern void ILI9341_DrawCrop(const uint8_t *buffer, uint32_t nbytes, uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2);
extern void ILI9341_DrawFrame(const uint8_t *fb_addr, uint32_t nbytes);
extern uint8_t ILI9341_IsBusy(void);
extern void ILI9341_FillRect(uint32_t rgb888, uint32_t x_start, uint32_t x_width,uint32_t y_start,uint32_t y_height);
extern void *ILI9341_GetDrawBuffer1Addr(void);
extern void *ILI9341_GetDrawBuffer2Addr(void);
extern void ILI9341_Read_GRAM(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t *buffer);
extern void Draw_BoundingBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
extern void Parse_SSD_Single_Output(float* data[]);
extern void Parse_SSD_Multi_Output(float* data[]);
extern void RGB565_to_UINT8(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height);
extern void Crop_Center_192x192(const uint8_t *src, uint8_t *dst);
void LCD_PrintString(int x, int y, const char* str);
void LCD_PrintStringColor(int x, int y, const char* str, uint16_t color_rgb565);
void Draw_Rectangle_Outline(uint32_t x, uint32_t y, uint32_t  width, uint32_t height, uint16_t color_rgb565);
void Draw_Crosshair(uint32_t cx, uint32_t cy, uint32_t r, uint16_t color_rgb565);
void Draw_Target_Marker(uint32_t cx, uint32_t cy, uint32_t size, uint16_t color_rgb565);
void ILI9341_SetTargetFB(uint8_t *fb);
void ILI9341_FillRect_DMA2D(uint16_t color_rgb565, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void LCD_DrawChar_DMA2D(uint32_t x, uint32_t y, char c, uint16_t color_rgb565) ;
void Crop_and_Convert_RGB565_to_RGB888(const uint8_t *src, uint8_t *dst, uint32_t src_width, uint32_t src_height, uint32_t dst_width, uint32_t dst_height);
void Crop_and_Convert_Fast(const uint8_t *src, uint8_t *dst);
void Parse_FOMO_Output(const int8_t *output, uint32_t output_size);

extern int ai_x[10], ai_y[10], ai_w[10], ai_h[10];
extern float ai_score[10];
extern uint32_t ai_time_ms;
extern volatile int ai_detection_count;
extern volatile float ai_peak_score;
extern volatile int8_t ai_peak_tgt, ai_peak_bg;
extern volatile int ai_peak_gx, ai_peak_gy;
extern volatile int ai_cells_over_bg, ai_cells_over_conf;
/******************************************************************************
 *               HAL callbacks for DMAx_Streamx_IRQHandler                    *
 ******************************************************************************/

extern void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
extern void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);

#endif /* ILI9341_H_ */
