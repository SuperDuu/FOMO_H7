/*
 * ILI9341.c
 * ILI9341 SPI DMA Driver
 * Created on: Aug 5, 2024
 *     Author: K.Rudenko
 */

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

#include <OV7670.h>
#include "ILI9341.h"
#include "main.h"
#include "string.h"
#include "ai_platform.h"
#include <stdlib.h>
/******************************************************************************
 *                               LOCAL MACRO                                  *
 ******************************************************************************/


//------------------USER---------------------


#include "network_1752296348456.h"
#include "network_1752296348456_data.h"
#include "stdio.h"
#include "string.h"
//extern ai_handle network;
//ai_handle network = AI_HANDLE_NULL;
ai_error err;
#define IMG_W   224
#define IMG_H   224
#define IMG_CH  3

//-------------------------------------------


#define ILI9341_USE_DMA_CMSIS           0
#define ILI9341_MADCTL_MY               0x80 // Bottom to top
#define ILI9341_MADCTL_MX               0x40 // Right to left
#define ILI9341_MADCTL_MV               0x20 // Reverse Mode
#define ILI9341_MADCTL_ML               0x10 // LCD refresh Bottom to top
#define ILI9341_MADCTL_RGB              0x00 // Red-Green-Blue pixel order
#define ILI9341_MADCTL_BGR              0x08 // Blue-Green-Red pixel order
#define ILI9341_MADCTL_MH               0x04 // LCD refresh right to left

#define ILI9341_BYTES_TO_PIXELS(bytes)  ((bytes) / ILI9341.pixel_format)
#define ILI9341_PIXELS_TO_BYTES(pixels) ((pixels) * ILI9341.pixel_format)
#define ILI9341_FRAME_SIZE(w, h)        ((w) * (h) * ILI9341.pixel_format)

#ifndef UNUSED
#define UNUSED(x)                       (void)x
#endif

#define HIGH_16(x)                      ((((uint16_t)x) >> 8U) & 0xFFU)
#define LOW_16(x)                       ((((uint16_t)x) >> 0U) & 0xFFU)

#define ILI9341_RESX_HIGH()             SET_BIT(ILI9341_GPIO_PORT_RESX->BSRR,ILI9341_GPIO_PIN_RESX)
#define ILI9341_RESX_LOW()              SET_BIT(ILI9341_GPIO_PORT_RESX->BSRR,((uint32_t)ILI9341_GPIO_PIN_RESX << 16U))

#if (ILI9341_SPI_CS_HW_MANAGE == 1)
#define ILI9341_CSX_HIGH()
#define ILI9341_CSX_LOW()
#else
#define ILI9341_CSX_HIGH()              SET_BIT(ILI9341_GPIO_PORT_CSX->BSRR,ILI9341_GPIO_PIN_CSX)
#define ILI9341_CSX_LOW()               SET_BIT(ILI9341_GPIO_PORT_CSX->BSRR,((uint32_t)ILI9341_GPIO_PIN_CSX << 16U))
#endif

#define ILI9341_DCX_HIGH()              SET_BIT(ILI9341_GPIO_PORT_DCX->BSRR,ILI9341_GPIO_PIN_DCX)
#define ILI9341_DCX_LOW()               SET_BIT(ILI9341_GPIO_PORT_DCX->BSRR,((uint32_t)ILI9341_GPIO_PIN_DCX << 16U))

#define ILI9341_SPI_SET_8_BIT()                           \
    do                                                    \
    {                                                     \
            if (ILI9341.hspi->Init.DataSize == SPI_DATASIZE_16BIT)\
    {\
        ILI9341.hspi->Init.DataSize = SPI_DATASIZE_8BIT;\
        HAL_SPI_Init(ILI9341.hspi);\
    }                                         \
    } while(0U)


#define ILI9341_SPI_SET_16_BIT()                          \
    do                                                    \
    {                                                     \
            if (ILI9341.hspi->Init.DataSize == SPI_DATASIZE_8BIT)\
    {\
        ILI9341.hspi->Init.DataSize = SPI_DATASIZE_16BIT;\
        HAL_SPI_Init(ILI9341.hspi);\
    }                                         \
    } while(0U)

// TODO: add timeouts
#define ILI9341_CHECK_SPI(spi_handler_ptr)         do{\
    while(HAL_SPI_STATE_READY != HAL_SPI_GetState(spi_handler_ptr));\
    }while(0)

#define ILI9341_DATAWIDTH_8BIT          (1U)
#define ILI9341_DATAWIDTH_16BIT         (2U)
#define ILI9341_DATAWIDTH               ILI9341_DATAWIDTH_16BIT

#define ILI9341_DMA_MAX_ITEMS           (0xFFFFU)

/* Data buffer size */
#define DB_SIZE                         (20UL * 1024UL)

/******************************************************************************
 *                            ILI9341 REGISTERS                               *
 ******************************************************************************/

/* Level 1 Commands */
#define ILI9341_SWRESET                  0x01U   /* Software Reset */
#define ILI9341_READ_DISPLAY_ID          0x04U   /* Read display identification information */
#define ILI9341_RDDST                    0x09U   /* Read Display Status */
#define ILI9341_RDDPM                    0x0AU   /* Read Display Power Mode */
#define ILI9341_RDDMADCTL                0x0BU   /* Read Display MADCTL */
#define ILI9341_RDDCOLMOD                0x0CU   /* Read Display Pixel Format */
#define ILI9341_RDDIM                    0x0DU   /* Read Display Image Format */
#define ILI9341_RDDSM                    0x0EU   /* Read Display Signal Mode */
#define ILI9341_RDDSDR                   0x0FU   /* Read Display Self-Diagnostic Result */
#define ILI9341_SPLIN                    0x10U   /* Enter Sleep Mode */
#define ILI9341_SLEEP_OUT                0x11U   /* Sleep out register */
#define ILI9341_PTLON                    0x12U   /* Partial Mode ON */
#define ILI9341_NORMAL_MODE_ON           0x13U   /* Normal Display Mode ON */
#define ILI9341_DINVOFF                  0x20U   /* Display Inversion OFF */
#define ILI9341_DINVON                   0x21U   /* Display Inversion ON */
#define ILI9341_GAMMA                    0x26U   /* Gamma register */
#define ILI9341_DISPLAY_OFF              0x28U   /* Display off register */
#define ILI9341_DISPLAY_ON               0x29U   /* Display on register */
#define ILI9341_CASET                    0x2AU   /* Column address register */
#define ILI9341_RASET                    0x2BU   /* Page address register */
#define ILI9341_GRAM                     0x2CU   /* GRAM register */
#define ILI9341_RGBSET                   0x2DU   /* Color SET */
#define ILI9341_RAMRD                    0x2EU   /* Memory Read */
#define ILI9341_PLTAR                    0x30U   /* Partial Area */
#define ILI9341_VSCRDEF                  0x33U   /* Vertical Scrolling Definition */
#define ILI9341_TEOFF                    0x34U   /* Tearing Effect Line OFF */
#define ILI9341_TEON                     0x35U   /* Tearing Effect Line ON */
#define ILI9341_MAC                      0x36U   /* Memory Access Control register*/
#define ILI9341_VSCRSADD                 0x37U   /* Vertical Scrolling Start Address */
#define ILI9341_IDMOFF                   0x38U   /* Idle Mode OFF */
#define ILI9341_IDMON                    0x39U   /* Idle Mode ON */
#define ILI9341_PIXEL_FORMAT             0x3AU   /* Pixel Format register */
#define ILI9341_WRITE_MEM_CONTINUE       0x3CU   /* Write Memory Continue */
#define ILI9341_READ_MEM_CONTINUE        0x3EU   /* Read Memory Continue */
#define ILI9341_SET_TEAR_SCANLINE        0x44U   /* Set Tear Scanline */
#define ILI9341_GET_SCANLINE             0x45U   /* Get Scanline */
#define ILI9341_WDB                      0x51U   /* Write Brightness Display register */
#define ILI9341_RDDISBV                  0x52U   /* Read Display Brightness */
#define ILI9341_WCD                      0x53U   /* Write Control Display register*/
#define ILI9341_RDCTRLD                  0x54U   /* Read CTRL Display */
#define ILI9341_WRCABC                   0x55U   /* Write Content Adaptive Brightness Control */
#define ILI9341_RDCABC                   0x56U   /* Read Content Adaptive Brightness Control */
#define ILI9341_WRITE_CABC               0x5EU   /* Write CABC Minimum Brightness */
#define ILI9341_READ_CABC                0x5FU   /* Read CABC Minimum Brightness */
#define ILI9341_READ_ID1                 0xDAU   /* Read ID1 */
#define ILI9341_READ_ID2                 0xDBU   /* Read ID2 */
#define ILI9341_READ_ID3                 0xDCU   /* Read ID3 */

/* Level 2 Commands */
#define ILI9341_RGB_INTERFACE            0xB0U   /* RGB Interface Signal Control */
#define ILI9341_FRMCTR1                  0xB1U   /* Frame Rate Control (In Normal Mode) */
#define ILI9341_FRMCTR2                  0xB2U   /* Frame Rate Control (In Idle Mode) */
#define ILI9341_FRMCTR3                  0xB3U   /* Frame Rate Control (In Partial Mode) */
#define ILI9341_INVTR                    0xB4U   /* Display Inversion Control */
#define ILI9341_BPC                      0xB5U   /* Blanking Porch Control register */
#define ILI9341_DFC                      0xB6U   /* Display Function Control register */
#define ILI9341_ETMOD                    0xB7U   /* Entry Mode Set */
#define ILI9341_BACKLIGHT1               0xB8U   /* Backlight Control 1 */
#define ILI9341_BACKLIGHT2               0xB9U   /* Backlight Control 2 */
#define ILI9341_BACKLIGHT3               0xBAU   /* Backlight Control 3 */
#define ILI9341_BACKLIGHT4               0xBBU   /* Backlight Control 4 */
#define ILI9341_BACKLIGHT5               0xBCU   /* Backlight Control 5 */
#define ILI9341_BACKLIGHT7               0xBEU   /* Backlight Control 7 */
#define ILI9341_BACKLIGHT8               0xBFU   /* Backlight Control 8 */
#define ILI9341_POWER1                   0xC0U   /* Power Control 1 register */
#define ILI9341_POWER2                   0xC1U   /* Power Control 2 register */
#define ILI9341_VCOM1                    0xC5U   /* VCOM Control 1 register */
#define ILI9341_VCOM2                    0xC7U   /* VCOM Control 2 register */
#define ILI9341_NVMWR                    0xD0U   /* NV Memory Write */
#define ILI9341_NVMPKEY                  0xD1U   /* NV Memory Protection Key */
#define ILI9341_RDNVM                    0xD2U   /* NV Memory Status Read */
#define ILI9341_READ_ID4                 0xD3U   /* Read ID4 */
#define ILI9341_PGAMMA                   0xE0U   /* Positive Gamma Correction register */
#define ILI9341_NGAMMA                   0xE1U   /* Negative Gamma Correction register */
#define ILI9341_DGAMCTRL1                0xE2U   /* Digital Gamma Control 1 */
#define ILI9341_DGAMCTRL2                0xE3U   /* Digital Gamma Control 2 */
#define ILI9341_INTERFACE                0xF6U   /* Interface control register */

/* Extend register commands */
#define ILI9341_POWERA                   0xCBU   /* Power control A register */
#define ILI9341_POWERB                   0xCFU   /* Power control B register */
#define ILI9341_DTCA                     0xE8U   /* Driver timing control A */
#define ILI9341_DTCB                     0xEAU   /* Driver timing control B */
#define ILI9341_POWER_SEQ                0xEDU   /* Power on sequence register */
#define ILI9341_3GAMMA_EN                0xF2U   /* 3 Gamma enable register */
#define ILI9341_PRC                      0xF7U   /* Pump ratio control register */

/******************************************************************************
 *                           LOCAL DATA TYPES                                 *
 ******************************************************************************/

typedef struct
{
    uint16_t x1;
    uint16_t x2;
    uint16_t y1;
    uint16_t y2;
} lcd_area_t;

typedef struct
{
    uint32_t chunk_cnt;
    uint32_t src_address;
    uint32_t n_chunks;
    uint32_t nitems;
} spi_dma_runtime_t;

/******************************************************************************
 *                         LOCAL DATA PROTOTYPES                              *
 ******************************************************************************/

static volatile struct
{
    uint8_t           orientation;
    uint8_t           pixel_format;
    uint8_t           *draw_buffer1;
    uint8_t           *draw_buffer2;
    uint32_t          write_length;
    const uint8_t     *buff_to_draw;
    const uint8_t     *buff_to_flush;
    lcd_area_t        area;
    spi_dma_runtime_t spi_dma;
    ILI9341_FncPtr_t  dma_cplt_cb;
    ILI9341_FncPtr_t  dma_err_cb;
    SPI_HandleTypeDef *hspi;
} ILI9341;

enum
{
    FALSE, TRUE
};


uint8_t bsp_db[DB_SIZE];
uint8_t bsp_wb[DB_SIZE];

/******************************************************************************
 *                       LOCAL FUNCTIONS PROTOTYPES                           *
 ******************************************************************************/

static void lcd_Reset(void);
static void lcd_Config(void);
static void lcd_SetOrientation(uint8_t orientation);
static void lcd_BufferInit(void);
static uint8_t lcd_IsWriteAllowed(void);
static void lcd_SetDisplArea(void);
static uint8_t* lcd_GetBuff(void);
static void lcd_Flush(void);
static void lcd_WriteCmd(uint8_t cmd, const uint8_t *params, uint32_t param_len);
static void lcd_WriteDma(uint32_t src_addr, uint32_t nbytes);
static uint32_t lcd_CpyToDrawBuffer(uint32_t nbytes, uint32_t rgb888);
static void lcd_MakeArea(uint32_t x_start, uint32_t x_width, uint32_t y_start, uint32_t y_height);
static HAL_StatusTypeDef lcd_SPI_Send(SPI_HandleTypeDef *hspi, uint8_t *data, uint8_t len, uint32_t timeout);
//static HAL_StatusTypeDef lcd_SPI_Read(SPI_HandleTypeDef *hspi, uint8_t *data, uint8_t len, uint32_t timeout);
static HAL_StatusTypeDef lcd_SPI_SendRead(SPI_HandleTypeDef *hspi, uint8_t *rxData, uint8_t *txData, uint8_t len, uint32_t timeout);

void Draw_BoundingBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void RGB565_to_INT8(const uint8_t *src, int8_t *dst, uint32_t width, uint32_t height);
static uint16_t lcd_RGB888toRGB565(uint32_t rgb888);

#if (ILI9341_USE_DMA_CMSIS == 1)
static void spi_transmit_dma(SPI_HandleTypeDef *hspi, const uint8_t *pData, uint16_t Size);
static void SPI_DMA_TxCpltCallback(DMA_HandleTypeDef *hdma);
#endif
/******************************************************************************
 *                              GLOBAL FUNCTIONS                              *
 ******************************************************************************/
extern struct
{
    /* HAL peripheral handlers */
    DCMI_HandleTypeDef  *hdcmi;
    I2C_HandleTypeDef   *hi2c;
    TIM_HandleTypeDef   *htim;
    uint32_t            tim_ch;
    /* Requested mode */
    uint32_t            mode;
    /* Address of the buffer */
    volatile uint32_t   buffer_addr[2];
    /* Image line counter */
    volatile uint32_t   lineCnt;
    /* Driver status */
    volatile uint8_t    state;
    /* Draw line callback prototype */
//    drawLine_cb_t       drawLine_cb;
    /* Draw frame callback prototype */
//    drawFrame_cb_t      drawFrame_cb;
    uint32_t frameCount;
     uint32_t 			lastTick;
     float 				fps;

} OV7670;

extern DMA2D_HandleTypeDef hdma2d;

#define LCD_W 320
#define LCD_H 240
#define MODEL_INPUT_W 192
#define MODEL_INPUT_H 192
#define OFFSET_X ((LCD_W - MODEL_INPUT_W)/2)
#define OFFSET_Y ((LCD_H - MODEL_INPUT_H)/2)
float score;
float *check;
#define NUM_BOXES 3830
float outt1;
float outt2;
float outt3;
float outt0;
float outt11;
float outt21;
float outt31;
float outt01;
int box_w ;
int box_h;
float cx ;
float cy;
#define MAX_DETECTIONS 10
#define NMS_THRESHOLD 0.1f
#define CONF_THRESHOLD 0.6f

typedef struct {
    int x, y;       // Tọa độ top-left
    int width;      // Chiều rộng
    int height;     // Chiều cao
    float score;    // Điểm tin cậy
    int suppressed;
} Detection;
Detection detections[MAX_DETECTIONS];
int detection_count = 0;


void RGB565_to_UINT8(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height)
{
    for (uint32_t i = 0; i < width * height; i++)
    {
        uint16_t pixel = src[i*2] | (src[i*2+1] << 8);
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        r |= r >> 5;

        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        g |= g >> 6;

        uint8_t b = (pixel & 0x1F) << 3;
        b |= b >> 5;
        dst[i*3 + 0] = r;
        dst[i*3 + 1] = g;
        dst[i*3 + 2] = b;
    }
}

void Crop_and_Convert_RGB565_to_RGB888(const uint8_t *src, uint8_t *dst, uint32_t src_width, uint32_t src_height, uint32_t dst_width, uint32_t dst_height)
{
    uint32_t start_x = (src_width - dst_width) / 2;
    uint32_t start_y = (src_height - dst_height) / 2;

    for (uint32_t y = 0; y < dst_height; y++)
    {
        for (uint32_t x = 0; x < dst_width; x++)
        {
            uint32_t src_idx = ((start_y + y) * src_width + (start_x + x)) * 2;
            uint32_t dst_idx = (y * dst_width + x) * 3;

            uint16_t pixel = src[src_idx] | (src[src_idx + 1] << 8);

            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            r |= r >> 5;

            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            g |= g >> 6;

            uint8_t b = (pixel & 0x1F) << 3;
            b |= b >> 5;


            dst[dst_idx + 0] = r;
            dst[dst_idx + 1] = g;
            dst[dst_idx + 2] = b;
        }
    }
}

/* ===========================================================================
 * Crop_and_Convert_Fast: Merge crop + RGB565->RGB888 in ONE pass.
 * Reads 2 pixels per 32-bit word. Skips bit-fill for speed.
 * Eliminates the intermediate frame_192x192 buffer entirely.
 * =========================================================================== */
/* Crop 240x240 at screen center, resize to 128x128, RGB565 -> int8 RGB */
void Crop_and_Convert_Fast(const uint8_t *src, uint8_t *dst)
{
    const uint32_t start_x = (SRC_WIDTH - CROP_WIDTH) / 2; // (320 - 240) / 2 = 40
    const uint32_t start_y = (SRC_HEIGHT - CROP_HEIGHT) / 2; // (240 - 240) / 2 = 0

    for (uint32_t y = 0; y < DST_HEIGHT; y++)
    {
        /* 240 / 128 = 15 / 8: fast scaling without integer division */
        const uint32_t src_y = start_y + ((y * 15U) >> 3);
        const uint32_t row_offset = src_y * SRC_WIDTH;
        int8_t *d = (int8_t*)(dst + y * DST_WIDTH * 3);

        for (uint32_t x = 0; x < DST_WIDTH; x++)
        {
            const uint32_t src_x = start_x + ((x * 15U) >> 3);
            const uint32_t src_idx = (row_offset + src_x) * 2U;

            /* OV7670 RGB565 in Little-Endian Memory:
             * Byte 0 (src_idx):     Low byte  [G2 G1 G0 B4 B3 B2 B1 B0]
             * Byte 1 (src_idx + 1): High byte [R4 R3 R2 R1 R0 G5 G4 G3]
             */
            uint8_t b1 = src[src_idx];       /* Low byte */
            uint8_t b0 = src[src_idx + 1U];  /* High byte */

            uint8_t r = (uint8_t)(b0 & 0xF8U);
            r |= (uint8_t)(r >> 5);

            uint8_t g = (uint8_t)(((b0 & 0x07U) << 5) | ((b1 & 0xE0U) >> 3));
            g |= (uint8_t)(g >> 6);

            uint8_t b = (uint8_t)((b1 & 0x1FU) << 3);
            b |= (uint8_t)(b >> 5);

            *d++ = (int8_t)((int16_t)r - 128);
            *d++ = (int8_t)((int16_t)g - 128);
            *d++ = (int8_t)((int16_t)b - 128);
        }
    }
}
void Draw_BoundingBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ILI9341_FillRect(color, x, w, y, 1);
    ILI9341_FillRect(color, x, w, y + h - 1, 1);
    ILI9341_FillRect(color, x, 1, y, h);
    ILI9341_FillRect(color, x + w - 1, 1, y, h);
}


const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 0x20 ' '
    {0x00,0x00,0x5F,0x00,0x00}, // 0x21 '!'
    {0x00,0x07,0x00,0x07,0x00}, // 0x22 '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // 0x23 '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // 0x24 '$'
    {0x23,0x13,0x08,0x64,0x62}, // 0x25 '%'
    {0x36,0x49,0x55,0x22,0x50}, // 0x26 '&'
    {0x00,0x05,0x03,0x00,0x00}, // 0x27 '''
    {0x00,0x1C,0x22,0x41,0x00}, // 0x28 '('
    {0x00,0x41,0x22,0x1C,0x00}, // 0x29 ')'
    {0x14,0x08,0x3E,0x08,0x14}, // 0x2A '*'
    {0x08,0x08,0x3E,0x08,0x08}, // 0x2B '+'
    {0x00,0x50,0x30,0x00,0x00}, // 0x2C ','
    {0x08,0x08,0x08,0x08,0x08}, // 0x2D '-'
    {0x00,0x60,0x60,0x00,0x00}, // 0x2E '.'
    {0x20,0x10,0x08,0x04,0x02}, // 0x2F '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // 0x30 '0'
    {0x00,0x42,0x7F,0x40,0x00}, // 0x31 '1'
    {0x42,0x61,0x51,0x49,0x46}, // 0x32 '2'
    {0x21,0x41,0x45,0x4B,0x31}, // 0x33 '3'
    {0x18,0x14,0x12,0x7F,0x10}, // 0x34 '4'
    {0x27,0x45,0x45,0x45,0x39}, // 0x35 '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // 0x36 '6'
    {0x01,0x71,0x09,0x05,0x03}, // 0x37 '7'
    {0x36,0x49,0x49,0x49,0x36}, // 0x38 '8'
    {0x06,0x49,0x49,0x29,0x1E}, // 0x39 '9'
    {0x00,0x36,0x36,0x00,0x00}, // 0x3A ':'
    {0x00,0x56,0x36,0x00,0x00}, // 0x3B ';'
    {0x08,0x14,0x22,0x41,0x00}, // 0x3C '<'
    {0x14,0x14,0x14,0x14,0x14}, // 0x3D '='
    {0x00,0x41,0x22,0x14,0x08}, // 0x3E '>'
    {0x02,0x01,0x51,0x09,0x06}, // 0x3F '?'
    {0x32,0x49,0x79,0x41,0x3E}, // 0x40 '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 0x41 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 0x42 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 0x43 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 0x44 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 0x45 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 0x46 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 0x47 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 0x48 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 0x49 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 0x4A 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 0x4B 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 0x4C 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 0x4D 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 0x4E 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 0x4F 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 0x50 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 0x51 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 0x52 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 0x53 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 0x54 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 0x55 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 0x56 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 0x57 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 0x58 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 0x59 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 0x5A 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // 0x5B '['
    {0x02,0x04,0x08,0x10,0x20}, // 0x5C '\'
    {0x00,0x41,0x41,0x7F,0x00}, // 0x5D ']'
    {0x04,0x02,0x01,0x02,0x04}, // 0x5E '^'
    {0x40,0x40,0x40,0x40,0x40}, // 0x5F '_'
    {0x00,0x01,0x02,0x04,0x00}, // 0x60 '`'
    {0x20,0x54,0x54,0x54,0x78}, // 0x61 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 0x62 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 0x63 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 0x64 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 0x65 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 0x66 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 0x67 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 0x68 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 0x69 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 0x6A 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 0x6B 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 0x6C 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 0x6D 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 0x6E 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 0x6F 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 0x70 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 0x71 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 0x72 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 0x73 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 0x74 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 0x75 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 0x76 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 0x77 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 0x78 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 0x79 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 0x7A 'z'
    {0x00,0x08,0x36,0x41,0x00}, // 0x7B '{'
    {0x00,0x00,0x7F,0x00,0x00}, // 0x7C '|'
    {0x00,0x41,0x36,0x08,0x00}, // 0x7D '}'
    {0x10,0x08,0x08,0x10,0x08}, // 0x7E '~'
    {0x00,0x00,0x00,0x00,0x00}  // 0x7F DEL
};
void LCD_DrawChar(int x, int y, char c, uint16_t color) {
    if (c < 32 || c > 127) c = '?'; // xử lý ký tự ngoài phạm vi

    const uint8_t* chr = font5x7[c - 32];

    for (int col = 0; col < 5; col++) {
        uint8_t bits = chr[col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                ILI9341_FillRect(color, x + col, 1, y + row, 1);  // vẽ 1 pixel
            }
        }
    }
}

void LCD_PrintStringColor(int x, int y, const char* str, uint16_t color_rgb565) {
    while (*str) {
    	LCD_DrawChar_DMA2D(x, y, *str, color_rgb565);
        x += 6;
        str++;
    }
}

void LCD_PrintString(int x, int y, const char* str) {
    LCD_PrintStringColor(x, y, str, 0xFFFF);
}


void LCD_DrawChar_DMA2D(uint32_t x, uint32_t y, char c, uint16_t color_rgb565) {
    if (c < 32 || c > 127) c = '?';

    const uint8_t* chr = font5x7[c - 32];

    for (uint32_t col = 0; col < 5; col++) {
        uint8_t bits = chr[col];
        uint32_t row = 0;

        while (row < 7) {
            if (bits & (1 << row)) {
                uint32_t start_row = row;
                uint32_t height = 0;

                while (row < 7 && (bits & (1 << row))) {
                    height++;
                    row++;
                }

                ILI9341_FillRect_DMA2D(color_rgb565, x + col, y + start_row, 1, height);
            } else {
                row++;
            }
        }
    }
}
float x_ofs, y_ofs;
extern int ai_x[10], ai_y[10], ai_w[10], ai_h[10];
extern float ai_score[10];
extern volatile int ai_detection_count;
extern volatile float ai_peak_score;
extern volatile int8_t ai_peak_tgt, ai_peak_bg;
extern volatile int ai_peak_gx, ai_peak_gy;
extern volatile int ai_cells_over_bg, ai_cells_over_conf;
//extern float best_score ;
void Parse_SSD_Single_Output(float* data[]) {
    float* class_scores = data[0];
    float* bboxes       = data[2];
    float best_score = 0.0f;  /* Fixed: was uninitialized! */
    int best_x = 0, best_y = 0, best_w = 0, best_h = 0;

    /* Optimized: pointer arithmetic instead of index * 2 / index * 4 */
    float *cs = class_scores + 1;  /* point to class 1 score */
    float *bb = bboxes;
    for (int i = 0; i < NUM_BOXES; i++, cs += 2, bb += 4) {
        float p_obj = *cs;
        if (p_obj < CONF_THRESHOLD) continue;

        float x_min = bb[0];
        float y_min = bb[1];
        float x_max = bb[2];
        float y_max = bb[3];

        int x = (int)(x_min * MODEL_INPUT_W) + OFFSET_X;
        int y = (int)(y_min * MODEL_INPUT_H) + OFFSET_Y;
        int bw = (int)((x_max - x_min) * MODEL_INPUT_W);
        int bh = (int)((y_max - y_min) * MODEL_INPUT_H);

        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (bw <= 0 || bh <= 0) continue;
        if (x + bw > LCD_W) bw = LCD_W - x;
        if (y + bh > LCD_H) bh = LCD_H - y;

        /* LỌC NHIỄU: Bỏ qua các box quá lớn (model bị lỗi nhận diện toàn màn hình khi không có vật thể) */
        if (bw > 250 || bh > 200) continue;

        if (p_obj > best_score) {
            best_score = p_obj;
            best_x = x;
            best_y = y;
            best_w = bw;
            best_h = bh;
        }
    }

    if (best_score > CONF_THRESHOLD) {
    	ai_x[0]=best_x;
    	ai_y[0]=best_y;
    	ai_w[0]=best_w;
    	ai_h[0]=best_h;
    	ai_score[0]=best_score;
		x_ofs=best_x+best_w*1.0f/2-160;
		y_ofs=best_y+best_h*1.0f/2-120;

    }
    else{
    	ai_x[0]=0;
		ai_y[0]=0;
		ai_w[0]=0;
		ai_h[0]=0;
		ai_score[0]=best_score;
		x_ofs=0;
		y_ofs=0;
    }
}

/* ===========================================================================
 * Parse_FOMO_Output: Parse 16x16x2 INT8 output tensor from X-CUBE-AI FOMO model.
 * 1. 8-connectivity BFS clustering (gom mọi ô lân cận kể cả đường chéo).
 * 2. Weighted centroid (tâm khối lượng chính xác theo xác suất ô).
 * 3. Distance-based NMS (triệt tiêu cụm điểm trùng lặp trong bán kính 30px).
 * 4. Atomic IRQ protection (chống xung đột race-condition với ngắt DCMI).
 * =========================================================================== */
void Parse_FOMO_Output(const int8_t *output, uint32_t output_size)
{
    if (output == NULL || output_size < (16 * 16 * 2)) {
        __disable_irq();
        for (int i = 0; i < 10; i++) {
            ai_x[i] = 0; ai_y[i] = 0; ai_w[i] = 0; ai_h[i] = 0;
            ai_score[i] = 0.0f;
        }
        ai_detection_count = 0;
        ai_peak_score = 0.0f;
        ai_peak_tgt = -128;
        ai_peak_bg = -128;
        ai_peak_gx = 0;
        ai_peak_gy = 0;
        ai_cells_over_bg = 0;
        ai_cells_over_conf = 0;
        __enable_irq();
        return;
    }

    /* Compute telemetry stats across the entire 16x16 grid */
    int8_t max_t = -128, max_b = -128;
    int max_gx = 0, max_gy = 0;
    int over_bg = 0, over_conf = 0;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int idx = (y * 16 + x) * 2;
            int8_t b_val = output[idx];
            int8_t t_val = output[idx + 1];
            if (t_val > b_val) over_bg++;
            float s = ((float)t_val + 128.0f) / 256.0f;
            if (s >= FOMO_CONF_THRESHOLD) over_conf++;
            if (t_val > max_t) {
                max_t = t_val;
                max_b = b_val;
                max_gx = x;
                max_gy = y;
            }
        }
    }

    uint8_t visited[16][16] = {0};

    #define MAX_FOMO_CANDS 16
    typedef struct {
        int x;
        int y;
        float score;
    } fomo_candidate_t;

    fomo_candidate_t cands[MAX_FOMO_CANDS];
    int cand_count = 0;

    /* 8 hướng lân cận (Bao gồm cả 4 đường chéo) để chống phân mảnh vật thể */
    static const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
    static const int dy[8] = {0, 0, -1, 1, -1, -1, 1, 1};

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (visited[y][x]) continue;

            int idx = (y * 16 + x) * 2;
            int8_t bg_val = output[idx];
            int8_t target_val = output[idx + 1];

            /* Phải đảm bảo xác suất Target vượt trội hơn Background */
            if (target_val <= bg_val) continue;

            float target_score = ((float)target_val + 128.0f) / 256.0f;
            if (target_score < FOMO_CONF_THRESHOLD) {
                continue;
            }

            /* BFS Flood-fill 8-connectivity gom trọn vẹn mọi ô của cùng 1 vật thể:
             * Áp dụng Hysteresis Thresholding: Ô kích hoạt ban đầu phải >= FOMO_CONF_THRESHOLD (0.80),
             * nhưng các ô lân cận trong cùng vật thể chỉ cần >= 0.40f là được gom chung vào một cụm duy nhất.
             */
            #define FOMO_PROPAGATION_THRESHOLD 0.40f
            int queue_x[128], queue_y[128];
            int head = 0, tail = 0;

            queue_x[tail] = x;
            queue_y[tail] = y;
            tail++;
            visited[y][x] = 1;

            int min_x = x, max_x = x;
            int min_y = y, max_y = y;
            float best_score = target_score;
            float sum_x = (float)x * target_score;
            float sum_y = (float)y * target_score;
            float sum_w = target_score;

            while (head < tail) {
                int cx = queue_x[head];
                int cy = queue_y[head];
                head++;

                for (int d = 0; d < 8; d++) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];

                    if (nx >= 0 && nx < 16 && ny >= 0 && ny < 16 && !visited[ny][nx]) {
                        int nidx = (ny * 16 + nx) * 2;
                        int8_t n_bg = output[nidx];
                        int8_t n_target = output[nidx + 1];

                        if (n_target > n_bg) {
                            float nscore = ((float)n_target + 128.0f) / 256.0f;

                            if (nscore >= FOMO_PROPAGATION_THRESHOLD) {
                                visited[ny][nx] = 1;
                                if (tail < 128) {
                                    queue_x[tail] = nx;
                                    queue_y[tail] = ny;
                                    tail++;
                                }
                                if (nx < min_x) min_x = nx;
                                if (nx > max_x) max_x = nx;
                                if (ny < min_y) min_y = ny;
                                if (ny > max_y) max_y = ny;
                                if (nscore > best_score) best_score = nscore;
                                sum_x += (float)nx * nscore;
                                sum_y += (float)ny * nscore;
                                sum_w += nscore;
                            }
                        }
                    }
                }
            }

            /* Bắt buộc cụm phải có đỉnh (peak score) đạt chuẩn ngưỡng tin cậy */
            if (best_score < FOMO_CONF_THRESHOLD) {
                continue;
            }

            /* Centroid chuẩn xác bằng tâm khối lượng có trọng số (Weighted Centroid) */
            float center_grid_x = (sum_w > 0.0f) ? (sum_x / sum_w) : ((float)(min_x + max_x) * 0.5f);
            float center_grid_y = (sum_w > 0.0f) ? (sum_y / sum_w) : ((float)(min_y + max_y) * 0.5f);
            int center_x = 40 + (int)(center_grid_x * 15.0f + 7.5f);
            int center_y = 0  + (int)(center_grid_y * 15.0f + 7.5f);
            if (center_x < 0) center_x = 0;
            if (center_x >= ILI9341_ACTIVE_WIDTH) center_x = ILI9341_ACTIVE_WIDTH - 1;
            if (center_y < 0) center_y = 0;
            if (center_y >= ILI9341_ACTIVE_HEIGHT) center_y = ILI9341_ACTIVE_HEIGHT - 1;

            /* Lọc bỏ cụm dị thường tràn màn hình */
            int span_w = (max_x - min_x + 1) * 15;
            int span_h = (max_y - min_y + 1) * 15;
            if (span_w > 180 || span_h > 180) continue;

            /* Lưu candidate và sắp xếp giảm dần theo điểm tin cậy (score) */
            if (cand_count < MAX_FOMO_CANDS) {
                int insert_pos = cand_count;
                for (int i = 0; i < cand_count; i++) {
                    if (best_score > cands[i].score) {
                        insert_pos = i;
                        break;
                    }
                }
                for (int i = cand_count; i > insert_pos; i--) {
                    cands[i] = cands[i-1];
                }
                cands[insert_pos].x = center_x;
                cands[insert_pos].y = center_y;
                cands[insert_pos].score = best_score;
                cand_count++;
            }
        }
    }

    /* Distance-based Non-Maximum Suppression (NMS):
     * Bán kính triệt tiêu 60 pixel (~4 ô lưới, đường kính 120px bao phủ trọn vẹn 1 vật thể).
     * Bất kỳ điểm nào nằm trong bán kính của một điểm mạnh hơn đều bị triệt tiêu,
     * ngăn chặn hoàn toàn việc sinh nhiều tâm xung quanh cùng 1 vật thể.
     */
    #define FOMO_SUPPRESS_DIST_SQ (60 * 60)
    fomo_candidate_t final_dets[5];
    int final_count = 0;

    for (int i = 0; i < cand_count && final_count < 5; i++) {
        int suppress = 0;
        for (int j = 0; j < final_count; j++) {
            int dx_dist = cands[i].x - final_dets[j].x;
            int dy_dist = cands[i].y - final_dets[j].y;
            if ((dx_dist * dx_dist + dy_dist * dy_dist) < FOMO_SUPPRESS_DIST_SQ) {
                suppress = 1;
                break;
            }
        }
        if (!suppress) {
            final_dets[final_count++] = cands[i];
        }
    }

    /* Sao chép nguyên tử (Atomic transfer) có khóa ngắt để ISR không bao giờ đọc trúng mảng dở dang */
    __disable_irq();
    ai_detection_count = final_count;
    ai_peak_tgt = max_t;
    ai_peak_bg = max_b;
    ai_peak_score = ((float)max_t + 128.0f) / 256.0f;
    ai_peak_gx = max_gx;
    ai_peak_gy = max_gy;
    ai_cells_over_bg = over_bg;
    ai_cells_over_conf = over_conf;
    for (int i = 0; i < 5; i++) {
        if (i < final_count) {
            ai_x[i] = final_dets[i].x;
            ai_y[i] = final_dets[i].y;
            ai_w[i] = 16;
            ai_h[i] = 16;
            ai_score[i] = final_dets[i].score;
        } else {
            ai_x[i] = 0;
            ai_y[i] = 0;
            ai_w[i] = 0;
            ai_h[i] = 0;
            ai_score[i] = 0.0f;
        }
    }
    for (int i = 5; i < 10; i++) {
        ai_score[i] = 0.0f;
    }
    __enable_irq();
}


void Parse_SSD_Multi_Output(float* data[]) {
    float* class_scores = data[0];  // [3830 x 2] softmax output
    float* bboxes       = data[2];  // [3830 x 4] [x_min, y_min, x_max, y_max]

    detection_count = 0;  // Reset detection count

    // Bước 1: Thu thập tất cả detection vượt ngưỡng
    for (int i = 0; i < NUM_BOXES && detection_count < MAX_DETECTIONS; i++) {
        float p_obj = class_scores[i * 2 + 1];  // Xác suất vật thể
        if (p_obj < CONF_THRESHOLD) continue;

        // Đọc tọa độ box (chuẩn hóa [0,1])
        float x_min = bboxes[i * 4 + 0];
        float y_min = bboxes[i * 4 + 1];
        float x_max = bboxes[i * 4 + 2];
        float y_max = bboxes[i * 4 + 3];

        // Chuyển đổi sang tọa độ pixel
        int x = (int)(x_min * MODEL_INPUT_W) + OFFSET_X;
        int y = (int)(y_min * MODEL_INPUT_H) + OFFSET_Y;
        int width = (int)((x_max - x_min) * MODEL_INPUT_W);
        int height = (int)((y_max - y_min) * MODEL_INPUT_H);

        // Kiểm tra kích thước hợp lệ
        if (width <= 5 || height <= 5) continue;
        if (x + width > LCD_W || y + height > LCD_H) continue;

        // Lưu detection
        detections[detection_count].x = x;
        detections[detection_count].y = y;
        detections[detection_count].width = width;
        detections[detection_count].height = height;
        detections[detection_count].score = p_obj;
        detection_count++;
    }

    // Bước 2: Áp dụng Non-Maximum Suppression (NMS)
    for (int i = 0; i < detection_count; i++) {
        for (int j = i + 1; j < detection_count; j++) {
            // Tính diện tích box
            int area_i = detections[i].width * detections[i].height;
            int area_j = detections[j].width * detections[j].height;

            // Tính tọa độ giao nhau
            int x_left = (detections[i].x > detections[j].x) ? detections[i].x : detections[j].x;
            int y_top = (detections[i].y > detections[j].y) ? detections[i].y : detections[j].y;
            int x_right = (detections[i].x + detections[i].width < detections[j].x + detections[j].width)
                          ? detections[i].x + detections[i].width : detections[j].x + detections[j].width;
            int y_bottom = (detections[i].y + detections[i].height < detections[j].y + detections[j].height)
                           ? detections[i].y + detections[i].height : detections[j].y + detections[j].height;

            // Tính diện tích giao nhau
            int inter_area = 0;
            if (x_right > x_left && y_bottom > y_top) {
                inter_area = (x_right - x_left) * (y_bottom - y_top);
            }

            // Tính IoU
            float iou = (float)inter_area / (area_i + area_j - inter_area);

            // Loại bỏ box có IoU cao và điểm thấp hơn
            if (iou > NMS_THRESHOLD) {
                if (detections[i].score > detections[j].score) {
                    detections[j].score = 0;
                } else {
                    detections[i].score = 0;
                }
            }
        }
    }

    // Bước 3: Vẽ tất cả detection hợp lệ
    for (int i = 0; i < detection_count; i++) {
        if (detections[i].score > CONF_THRESHOLD){
            ai_x[i]=detections[i].x;
    		ai_y[i]=detections[i].y;
    		ai_w[i]=detections[i].width;
    		ai_h[i]=detections[i].height;
    		ai_score[i]=detections[i].score;
        }
        else{
        	ai_x[i]=0;
    		ai_y[i]=0;
    		ai_w[i]=0;
    		ai_h[i]=0;
    		ai_score[i]=detections[i].score;
    		x_ofs=0;
    		y_ofs=0;
        }



    }
}
int final_detection_count = 0;
int compare_detections(const void* a, const void* b) {
    float score_a = ((Detection*)a)->score;
    float score_b = ((Detection*)b)->score;
    if (score_a < score_b) return 1;
    if (score_a > score_b) return -1;
    return 0;
}

float calculate_iou(Detection det1, Detection det2) {
    int x_left = (det1.x > det2.x) ? det1.x : det2.x;
    int y_top = (det1.y > det2.y) ? det1.y : det2.y;
    int x_right = (det1.x + det1.width < det2.x + det2.width) ? (det1.x + det1.width) : (det2.x + det2.width);
    int y_bottom = (det1.y + det1.height < det2.y + det2.height) ? (det1.y + det1.height) : (det2.y + det2.height);

    if (x_right <= x_left || y_bottom <= y_top) {
        return 0.0f;
    }

    int intersection_area = (x_right - x_left) * (y_bottom - y_top);
    int area1 = det1.width * det1.height;
    int area2 = det2.width * det2.height;

    return (float)intersection_area / (float)(area1 + area2 - intersection_area);
}
//void Parse_SSD_Multi_Output(float* data[]) {
//    float* class_scores = data[0]; // [3830 x 2] softmax output
//    float* bboxes       = data[2]; // [3830 x 4] [x_min, y_min, x_max, y_max]
//
//    Detection candidates[MAX_DETECTIONS];
//    int candidate_count = 0;
//
//    for (int i = 0; i < NUM_BOXES && candidate_count < MAX_DETECTIONS; i++) {
//        float p_obj = class_scores[i * 2 + 1];
//        if (p_obj < CONF_THRESHOLD) continue;
//
//        float x_min = bboxes[i * 4 + 0];
//        float y_min = bboxes[i * 4 + 1];
//        float x_max = bboxes[i * 4 + 2];
//        float y_max = bboxes[i * 4 + 3];
//
//        int x = (int)(x_min * MODEL_INPUT_W) + OFFSET_X;
//        int y = (int)(y_min * MODEL_INPUT_H) + OFFSET_Y;
//        int width = (int)((x_max - x_min) * MODEL_INPUT_W);
//        int height = (int)((y_max - y_min) * MODEL_INPUT_H);
//
//        if (width <= 5 || height <= 5) continue;
//        if (x + width > LCD_W || y + height > LCD_H) continue;
//
//        candidates[candidate_count].x = x;
//        candidates[candidate_count].y = y;
//        candidates[candidate_count].width = width;
//        candidates[candidate_count].height = height;
//        candidates[candidate_count].score = p_obj;
//        candidates[candidate_count].suppressed = 0;
//        candidate_count++;
//    }
//
//    qsort(candidates, candidate_count, sizeof(Detection), compare_detections);
//
//    for (int i = 0; i < candidate_count; i++) {
//        if (candidates[i].suppressed) continue;
//
//        for (int j = i + 1; j < candidate_count; j++) {
//            if (candidates[j].suppressed) continue;
//
//            float iou = calculate_iou(candidates[i], candidates[j]);
//            if (iou > NMS_THRESHOLD) {
//                candidates[j].suppressed = 1;
//            }
//        }
//    }
//
//    final_detection_count = 0;
//    for (int i = 0; i < candidate_count; i++) {
//        if (!candidates[i].suppressed) {
//            ai_x[final_detection_count] = candidates[i].x;
//            ai_y[final_detection_count] = candidates[i].y;
//            ai_w[final_detection_count] = candidates[i].width;
//            ai_h[final_detection_count] = candidates[i].height;
//            ai_score[final_detection_count] = candidates[i].score;
//            final_detection_count++;
//        }
//        else{
//        	ai_x[i]=0;
//    		ai_y[i]=0;
//    		ai_w[i]=0;
//    		ai_h[i]=0;
//    		ai_score[i]=0;
//        }
//    }
//}
void ILI9341_Init(SPI_HandleTypeDef *spi_handle, ILI9341_PixFormat_t pixFormat)
{
    ILI9341.hspi = spi_handle;
    ILI9341.orientation = ILI9341_ORIENTATION;
    ILI9341.pixel_format = pixFormat;
    lcd_Reset();
    lcd_Config();
    ILI9341.area.x1 = 0;
    ILI9341.area.x2 = ILI9341_ACTIVE_WIDTH - 1;
    ILI9341.area.y1 = 0;
    ILI9341.area.y2 = ILI9341_ACTIVE_HEIGHT - 1;
    lcd_SetDisplArea();
    lcd_SetOrientation(ILI9341.orientation);
    lcd_BufferInit();

#if (ILI9341_USE_DMA_CMSIS == 1)
    /**************************************************************
     * DMA post-configuration (moved from HAL_SPI_Transmit_DMA() API
     **************************************************************/
    /* Set the SPI TxDMA transfer complete callback */
    ILI9341.hspi->hdmatx->XferCpltCallback = SPI_DMA_TxCpltCallback;
    /* Disable double bufferization */
    CLEAR_BIT(ILI9341.hspi->hdmatx->Instance->CR, DMA_SxCR_DBM);
    /* Configure DMA Stream destination address */
    WRITE_REG(ILI9341.hspi->hdmatx->Instance->PAR, (uint32_t) &ILI9341.hspi->Instance->DR);
    __HAL_DMA_CLEAR_FLAG(ILI9341.hspi->hdmatx, __HAL_DMA_GET_TC_FLAG_INDEX(ILI9341.hspi->hdmatx));
    __HAL_DMA_CLEAR_FLAG(ILI9341.hspi->hdmatx, __HAL_DMA_GET_HT_FLAG_INDEX(ILI9341.hspi->hdmatx));
    __HAL_DMA_CLEAR_FLAG(ILI9341.hspi->hdmatx, __HAL_DMA_GET_TE_FLAG_INDEX(ILI9341.hspi->hdmatx));
    __HAL_DMA_CLEAR_FLAG(ILI9341.hspi->hdmatx, __HAL_DMA_GET_DME_FLAG_INDEX(ILI9341.hspi->hdmatx));
    __HAL_DMA_CLEAR_FLAG(ILI9341.hspi->hdmatx, __HAL_DMA_GET_FE_FLAG_INDEX(ILI9341.hspi->hdmatx));
    /* Enable Common interrupts*/
    SET_BIT(ILI9341.hspi->hdmatx->Instance->CR, (DMA_IT_TC | DMA_IT_TE | DMA_IT_DME));
#endif
}

void ILI9341_RegisterCallback(ILI9341_CB_t cb_type, ILI9341_FncPtr_t fnc_ptr)
{
    switch (cb_type)
    {
        case ILI9341_TC_CBK:
            ILI9341.dma_cplt_cb = fnc_ptr;
            break;

        case ILI9341_ERR_CBK:
            ILI9341.dma_err_cb = fnc_ptr;
            break;

        default:
            break;
    }
}

void Crop_Center_192x192(const uint8_t *src, uint8_t *dst)
{
    uint32_t start_x = (SRC_WIDTH - DST_WIDTH) / 2; // = 64
    uint32_t start_y = (SRC_HEIGHT - DST_HEIGHT) / 2; // = 24

    for (uint32_t y = 0; y < DST_HEIGHT; y++)
    {
        const uint8_t *src_row = src + ((start_y + y) * SRC_WIDTH + start_x) * 2;
        uint8_t *dst_row = dst + (y * DST_WIDTH) * 2;
        memcpy(dst_row, src_row, DST_WIDTH * 2);
    }
}



void ILI9341_DrawFrame(const uint8_t *fb_addr, uint32_t nbytes)
{
    ILI9341.area.x1 = 0U;
    ILI9341.area.x2 = ILI9341_ACTIVE_WIDTH - 1;//
    ILI9341.area.y1 = 0U;
    ILI9341.area.y2 = ILI9341_ACTIVE_HEIGHT - 1;//
    ILI9341.buff_to_flush = fb_addr;
    ILI9341.write_length = nbytes;
    lcd_Flush();


//    ILI9341.area.x1 = 0U;
//    ILI9341.area.x2 = DST_WIDTH - 1;//
//    ILI9341.area.y1 = 0U;
//    ILI9341.area.y2 = DST_HEIGHT - 1;//
//    ILI9341.buff_to_flush = fb_addr;
//    ILI9341.write_length = nbytes;
//    lcd_Flush();


//
//	ILI9341.area.x1 = (320 - DST_WIDTH) / 2; // 24
//	ILI9341.area.x2 = ILI9341.area.x1 + DST_WIDTH - 1;
//	ILI9341.area.y1 = (240 - DST_HEIGHT) / 2; // 64
//	ILI9341.area.y2 = ILI9341.area.y1 + DST_HEIGHT - 1;
//	ILI9341.buff_to_flush = fb_addr;
//	ILI9341.write_length = nbytes;
//	lcd_Flush();
}

uint8_t ILI9341_IsBusy(void)
{
    return (ILI9341.buff_to_flush != NULL) ? 1 : 0;
}

void ILI9341_DrawCrop(const uint8_t *buffer, uint32_t nbytes, uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2)
{
    ILI9341.area.x1 = x1;
    ILI9341.area.x2 = x2;
    ILI9341.area.y1 = y1;
    ILI9341.area.y2 = y2;
    ILI9341.buff_to_flush = buffer;
    ILI9341.write_length = nbytes;
    lcd_Flush();
}

void* ILI9341_GetDrawBuffer1Addr(void)
{
    return (void*) ILI9341.draw_buffer1;
}

void* ILI9341_GetDrawBuffer2Addr(void)
{
    return (void*) ILI9341.draw_buffer2;
}

void ILI9341_SetBackgroundColor(uint32_t rgb888)
{
    ILI9341_FillRect(rgb888, 0, ILI9341_ACTIVE_WIDTH, 0, ILI9341_ACTIVE_HEIGHT);
}

void ILI9341_FillRect(uint32_t rgb888, uint32_t x_start, uint32_t x_width, uint32_t y_start, uint32_t y_height)
{

    uint32_t total_bytes_to_write = 0;
    uint32_t bytes_sent_so_far = 0;
    uint32_t remaining_bytes = 0;
    uint32_t npix;
    uint32_t pixels_sent = 0;
    uint32_t x1, y1;
    uint32_t pixel_per_line = x_width;

    if ((x_start + x_width) > ILI9341_ACTIVE_WIDTH)
        return;

    if ((y_start + y_height) > ILI9341_ACTIVE_HEIGHT)
        return;

    // calculate total number of bytes written in to DB
    total_bytes_to_write = ILI9341_FRAME_SIZE(x_width, y_height);
    remaining_bytes = total_bytes_to_write;

    while (remaining_bytes)
    {
        x1 = x_start + (pixels_sent % pixel_per_line);
        y1 = y_start + (pixels_sent / pixel_per_line);

        lcd_MakeArea(x1, x_width, y1, y_height);

        if (x1 != x_start)
        {
            npix = x_start + x_width - x1;
        }
        else
        {
            npix = ILI9341_BYTES_TO_PIXELS(remaining_bytes);
        }

        bytes_sent_so_far += lcd_CpyToDrawBuffer(ILI9341_PIXELS_TO_BYTES(npix), rgb888);
        pixels_sent = ILI9341_BYTES_TO_PIXELS(bytes_sent_so_far);
        remaining_bytes = total_bytes_to_write - bytes_sent_so_far;
    }
}
//void ILI9341_FillRect_DMA2D(uint16_t color_rgb565, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
//    // Kiểm tra giới hạn tọa độ
//    if (x + w > ILI9341_ACTIVE_WIDTH || y + h > ILI9341_ACTIVE_HEIGHT) return;
//
//    // Tính toán địa chỉ đích trong framebuffer
//    uint32_t dest_addr = (uint32_t)OV7670.buffer_addr[0] + (y * ILI9341_ACTIVE_WIDTH + x) * sizeof(uint16_t);
//
//    // Cập nhật OutputOffset (thay đổi theo kích thước vùng vẽ)
//    DMA2D->OOR = ILI9341_ACTIVE_WIDTH - w;
//
//    // Thiết lập màu và bắt đầu truyền
////    HAL_DMA2D_SetOutputColor(&hdma2d, color_rgb565);
//    DMA2D->OCOLR = color_rgb565;
//    HAL_DMA2D_Start_IT(&hdma2d, color_rgb565, dest_addr, w, h);
////    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
//}

void ILI9341_FillRect_DMA2D(uint16_t color_rgb565, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    // Kiểm tra giới hạn tọa độ
    if (x + w > ILI9341_ACTIVE_WIDTH || y + h > ILI9341_ACTIVE_HEIGHT) return;

    // Tính toán địa chỉ đích trong framebuffer
    uint32_t dest_addr = (uint32_t)OV7670.buffer_addr[0] + (y * ILI9341_ACTIVE_WIDTH + x) * sizeof(uint16_t);

    // Cập nhật OutputOffset
    DMA2D->OOR = ILI9341_ACTIVE_WIDTH - w;

    // Thiết lập màu trực tiếp qua thanh ghi OCOLR
    DMA2D->OCOLR = color_rgb565;

    // Bắt đầu truyền
    HAL_DMA2D_Start(&hdma2d, color_rgb565, dest_addr, w, h);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
}

void Draw_Rectangle_Outline(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint16_t color_rgb565) {
    if (width == 0 || height == 0 || x + width > ILI9341_ACTIVE_WIDTH || y + height > ILI9341_ACTIVE_HEIGHT) return;

    ILI9341_FillRect_DMA2D(color_rgb565, x, y, width, 1);          // Cạnh trên
    ILI9341_FillRect_DMA2D(color_rgb565, x, y + height - 1, width, 1); // Cạnh dưới
    ILI9341_FillRect_DMA2D(color_rgb565, x, y, 1, height);         // Cạnh trái
    ILI9341_FillRect_DMA2D(color_rgb565, x + width - 1, y, 1, height); // Cạnh phải
}

/* Vẽ dấu crosshair '+' tại chính xác tâm vật thể (chuẩn FOMO) */
void Draw_Crosshair(uint32_t cx, uint32_t cy, uint32_t r, uint16_t color_rgb565) {
    uint32_t x1 = (cx >= r) ? (cx - r) : 0;
    uint32_t x2 = (cx + r < ILI9341_ACTIVE_WIDTH) ? (cx + r) : (ILI9341_ACTIVE_WIDTH - 1);
    uint32_t y1 = (cy >= r) ? (cy - r) : 0;
    uint32_t y2 = (cy + r < ILI9341_ACTIVE_HEIGHT) ? (cy + r) : (ILI9341_ACTIVE_HEIGHT - 1);

    if (x2 >= x1 && cy < ILI9341_ACTIVE_HEIGHT) {
        ILI9341_FillRect_DMA2D(color_rgb565, x1, cy, x2 - x1 + 1, 1);
    }
    if (y2 >= y1 && cx < ILI9341_ACTIVE_WIDTH) {
        ILI9341_FillRect_DMA2D(color_rgb565, cx, y1, 1, y2 - y1 + 1);
    }
}

/* Vẽ marker chuẩn FOMO: Crosshair tâm + ô vuông cố định kích thước size quanh tâm */
void Draw_Target_Marker(uint32_t cx, uint32_t cy, uint32_t size, uint16_t color_rgb565) {
    uint32_t half = size / 2;
    uint32_t x = (cx >= half) ? (cx - half) : 0;
    uint32_t y = (cy >= half) ? (cy - half) : 0;
    if (x + size >= ILI9341_ACTIVE_WIDTH) x = ILI9341_ACTIVE_WIDTH - size - 1;
    if (y + size >= ILI9341_ACTIVE_HEIGHT) y = ILI9341_ACTIVE_HEIGHT - size - 1;

    Draw_Rectangle_Outline(x, y, size, size, color_rgb565);
    Draw_Crosshair(cx, cy, half - 2, color_rgb565);
}

void ILI9341_Read_GRAM(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t *buffer)
{
    // Calculate the total number of pixels
    uint32_t num_pixels = (x2 - x1 + 1) * (y2 - y1 + 1);
    // placeholder for 2 18-bits RGB666 pixels, since ILI9341 is able to return only in this format
    uint8_t pixels_data[6] = { 0x00U };

    // Set the address window for the rectangle
    ILI9341.area.x1 = x1;
    ILI9341.area.x2 = x2;
    ILI9341.area.y1 = y1;
    ILI9341.area.y2 = y2;
    lcd_SetDisplArea();

    ILI9341_SPI_SET_8_BIT();

    // Pull CSX low before issuing the command
    ILI9341_CSX_LOW();
    // Switch to Command Mode
    ILI9341_DCX_LOW();

    uint8_t cmd = ILI9341_RAMRD;
    lcd_SPI_Send(ILI9341.hspi, &cmd, sizeof(cmd), HAL_MAX_DELAY);

    // Switch to Data Mode
    ILI9341_DCX_HIGH();

    // Generate dummy byte
    uint8_t dummy = 0x00U;
    lcd_SPI_Send(ILI9341.hspi, &dummy, sizeof(dummy), HAL_MAX_DELAY);

    while (num_pixels > 0U)
    {
        lcd_SPI_SendRead(ILI9341.hspi, pixels_data, pixels_data, sizeof(pixels_data), HAL_MAX_DELAY);
        /* Convert two pixels RGB666 to RGB888 */
        for (uint8_t i = 0U; i < sizeof(pixels_data); i += 3)
        {
            /* RGB666 -> RGB888 */
            *buffer++ = (pixels_data[i] & 0xFC) | (pixels_data[i] >> 6);  // Red
            *buffer++ = (pixels_data[i + 1] & 0xFC) | (pixels_data[i + 1] >> 6); // Green
            *buffer++ = (pixels_data[i + 2] & 0xFC) | (pixels_data[i + 2] >> 6); // Blue
        }

        num_pixels -= 2U;
    }

    // Pull CSX high manually after the read
    ILI9341_CSX_HIGH();
}

/******************************************************************************
 *                              LOCAL FUNCTIONS                               *
 ******************************************************************************/

static void lcd_SetOrientation(uint8_t orientation)
{
    uint8_t param;

    switch (orientation)
    {
        case ILI9341_LANDSCAPE:
        {
            /*Memory Access Control LANDSCAPE setting */
            param = ILI9341_MADCTL_MV | ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR;
            break;
        }

        case ILI9341_PORTRAIT:
        {
            /* Memory Access Control PORTRAIT setting */
            param = ILI9341_MADCTL_MY | ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR;
            break;
        }

        default:
        {
            break;
        }
    }

    /* Memory Access Control command */
    lcd_WriteCmd(ILI9341_MAC, &param, 1U);
}

static void lcd_Reset(void)
{
    ILI9341_RESX_LOW();
    ILI9341_DELAY(50);
    ILI9341_RESX_HIGH();
    ILI9341_DELAY(50);
}

static void lcd_Config(void)
{
    uint8_t params[15] = { 0U };

    params[0] = 0x00;
    params[1] = 0xD9;
    params[2] = 0x30;
    lcd_WriteCmd(ILI9341_SWRESET, NULL, 0U);
    lcd_WriteCmd(ILI9341_POWERB, params, 3U);

    params[0] = 0x64;
    params[1] = 0x03;
    params[2] = 0X12;
    params[3] = 0X81;
    lcd_WriteCmd(ILI9341_POWER_SEQ, params, 4U);

    params[0] = 0x85;
    params[1] = 0x10;
    params[2] = 0x7A;
    lcd_WriteCmd(ILI9341_DTCA, params, 3U);

    params[0] = 0x39;
    params[1] = 0x2C;
    params[2] = 0x00;
    params[3] = 0x34;
    params[4] = 0x02;
    lcd_WriteCmd(ILI9341_POWERA, params, 5U);

    params[0] = 0x20;
    lcd_WriteCmd(ILI9341_PRC, params, 1U);

    params[0] = 0x00;
    params[1] = 0x00;
    lcd_WriteCmd(ILI9341_DTCB, params, 2U);

    params[0] = 0x1B;
    lcd_WriteCmd(ILI9341_POWER1, params, 1U);

    params[0] = 0x12;
    lcd_WriteCmd(ILI9341_POWER2, params, 1U);

    params[0] = 0x08;
    params[1] = 0x26;
    lcd_WriteCmd(ILI9341_VCOM1, params, 2);

    params[0] = 0XB7;
    lcd_WriteCmd(ILI9341_VCOM2, params, 1);

    /* Select RGB565 */
    params[0] = 0x55;
    lcd_WriteCmd(ILI9341_PIXEL_FORMAT, params, 1);

    /* frame rate = 70 */
    params[0] = 0x00;
    params[1] = 0x1B;
    lcd_WriteCmd(ILI9341_FRMCTR1, params, 2);

    /* Display Function Control */
    params[0] = 0x0A;
    params[1] = 0xA2;
    lcd_WriteCmd(ILI9341_DFC, params, 2);

    /* 3Gamma Function Disable */
    params[0] = 0x02; //LCD_WR_DATA(0x00);
    lcd_WriteCmd(ILI9341_3GAMMA_EN, params, 1);

    params[0] = 0x01;
    lcd_WriteCmd(ILI9341_GAMMA, params, 1);

    /* Set Gamma */
    params[0] = 0x0F;
    params[1] = 0x1D;
    params[2] = 0x1A;
    params[3] = 0x0A;
    params[4] = 0x0D;
    params[5] = 0x07;
    params[6] = 0x49;
    params[7] = 0X66;
    params[8] = 0x3B;
    params[9] = 0x07;
    params[10] = 0x11;
    params[11] = 0x01;
    params[12] = 0x09;
    params[13] = 0x05;
    params[14] = 0x04;
    lcd_WriteCmd(ILI9341_PGAMMA, params, 15);

    params[0] = 0x00;
    params[1] = 0x18;
    params[2] = 0x1D;
    params[3] = 0x02;
    params[4] = 0x0F;
    params[5] = 0x04;
    params[6] = 0x36;
    params[7] = 0x13;
    params[8] = 0x4C;
    params[9] = 0x07;
    params[10] = 0x13;
    params[11] = 0x0F;
    params[12] = 0x2E;
    params[13] = 0x2F;
    params[14] = 0x05;
    lcd_WriteCmd(ILI9341_NGAMMA, params, 15);

    /* Exit Sleep */
    lcd_WriteCmd(ILI9341_SLEEP_OUT, NULL, 0U);

    ILI9341_DELAY(100);

    /* Display on */
    lcd_WriteCmd(ILI9341_DISPLAY_ON, NULL, 0U);
}

static void lcd_WriteCmd(uint8_t cmd, const uint8_t *params, uint32_t param_len)
{
	ILI9341_SPI_SET_8_BIT();
    ILI9341_CSX_LOW(); // SPI CS
    ILI9341_DCX_LOW(); // for commands
    /* HAL implementation: */
    HAL_SPI_Transmit(ILI9341.hspi, &cmd, 1U, HAL_MAX_DELAY);
    ILI9341_DCX_HIGH();// for commands
    if (params != NULL)
    {
        HAL_SPI_Transmit(ILI9341.hspi, (uint8_t*)params, param_len, HAL_MAX_DELAY);
    }

    ILI9341_CSX_HIGH(); // SPI CS
}

static void lcd_WriteDma(uint32_t src_addr, uint32_t nbytes)
{
    ILI9341_CHECK_SPI(ILI9341.hspi);

    /* Set SPI data width as 16 bits */
    ILI9341_SPI_SET_16_BIT();

    uint32_t nitems = nbytes / ILI9341_DATAWIDTH;

    // determine how many chunks of input buffer need to be sent
    ILI9341.spi_dma.n_chunks = (nitems % ILI9341_DMA_MAX_ITEMS != 0U) ?
                    (nitems / ILI9341_DMA_MAX_ITEMS + 1UL) :
                    (nitems / ILI9341_DMA_MAX_ITEMS);
    // assign DMA iteration counter
    ILI9341.spi_dma.chunk_cnt = ILI9341.spi_dma.n_chunks;
    // chunk size
    uint32_t chunk_size;

    /* Send all chunks via DMA */

    if (nitems > ILI9341_DMA_MAX_ITEMS)
    {
        chunk_size = ILI9341_DMA_MAX_ITEMS;
        nitems -= ILI9341_DMA_MAX_ITEMS;
    }
    else
    {
        chunk_size = nitems;
    }

    ILI9341.spi_dma.nitems = nitems;
    /* Shift source address */
    ILI9341.spi_dma.src_address = src_addr + ILI9341_DMA_MAX_ITEMS * (ILI9341.spi_dma.n_chunks - ILI9341.spi_dma.chunk_cnt) * ILI9341_DATAWIDTH;
    ILI9341.spi_dma.chunk_cnt--;

    ILI9341_CSX_LOW(); // SPI CS
#if (ILI9341_USE_DMA_CMSIS == 1)
    spi_transmit_dma(ILI9341.hspi, (uint8_t*) ILI9341.spi_dma.src_address, chunk_size);
#else
    HAL_SPI_Transmit_DMA(ILI9341.hspi, (uint8_t*) ILI9341.spi_dma.src_address, chunk_size);
#endif
}

static void lcd_SetDisplArea(void)
{
    uint8_t params[4];
    /*Column address set(2Ah) */
    params[0] = HIGH_16(ILI9341.area.x1);
    params[1] = LOW_16(ILI9341.area.x1);
    params[2] = HIGH_16(ILI9341.area.x2);
    params[3] = LOW_16(ILI9341.area.x2);
    lcd_WriteCmd(ILI9341_CASET, params, 4U);

    params[0] = HIGH_16(ILI9341.area.y1);
    params[1] = LOW_16(ILI9341.area.y1);
    params[2] = HIGH_16(ILI9341.area.y2);
    params[3] = LOW_16(ILI9341.area.y2);
    lcd_WriteCmd(ILI9341_RASET, params, 4U);
}

static void lcd_BufferInit(void)
{
    ILI9341.draw_buffer1 = bsp_db;
    ILI9341.draw_buffer2 = bsp_wb;
    ILI9341.buff_to_draw = NULL;
    ILI9341.buff_to_flush = NULL;
}

static uint16_t lcd_RGB888toRGB565(uint32_t rgb888)
{
    uint16_t r, g, b;
    r = (rgb888 >> 19) & 0x1FU;
    g = (rgb888 >> 10) & 0x3FU;
    b = (rgb888 >> 3) & 0x1FU;
    return (uint16_t) ((r << 11) | (g << 5) | b);
}

static void lcd_MakeArea(uint32_t x_start, uint32_t x_width, uint32_t y_start, uint32_t y_height)
{

    uint16_t lcd_total_width, lcd_total_height;

    lcd_total_width = ILI9341_ACTIVE_WIDTH - 1;
    lcd_total_height = ILI9341_ACTIVE_HEIGHT - 1;

    ILI9341.area.x1 = x_start;
    ILI9341.area.x2 = x_start + x_width - 1;
    ILI9341.area.y1 = y_start;
    ILI9341.area.y2 = y_start + y_height - 1;

    ILI9341.area.x2 = (ILI9341.area.x2 > lcd_total_width) ? lcd_total_width : ILI9341.area.x2;
    ILI9341.area.y2 = (ILI9341.area.y2 > lcd_total_height) ? lcd_total_height : ILI9341.area.y2;
}

static uint8_t* lcd_GetBuff(void)
{
    uint32_t buf1 = (uint32_t) ILI9341.draw_buffer1;
    uint32_t buf2 = (uint32_t) ILI9341.draw_buffer2;
    uint8_t *retPtr = NULL;

    __disable_irq();

    if (ILI9341.buff_to_draw == NULL && ILI9341.buff_to_flush == NULL)
    {
        retPtr = ILI9341.draw_buffer1;
    }
    else if ((uint32_t) ILI9341.buff_to_flush == buf1 && ILI9341.buff_to_draw == NULL)
    {
        retPtr = ILI9341.draw_buffer2;
    }
    else if ((uint32_t) ILI9341.buff_to_flush == buf2 && ILI9341.buff_to_draw == NULL)
    {
        retPtr = ILI9341.draw_buffer1;
    }
    __enable_irq();

    return retPtr;
}


static uint32_t lcd_CpyToDrawBuffer(uint32_t nbytes, uint32_t rgb888)
{
    uint16_t *fb_ptr = NULL;
    uint32_t npixels;
    ILI9341.buff_to_draw = lcd_GetBuff();
    fb_ptr = (uint16_t*) ILI9341.buff_to_draw;
    nbytes = ((nbytes > DB_SIZE) ? DB_SIZE : nbytes);
    npixels = ILI9341_BYTES_TO_PIXELS(nbytes);

    if (ILI9341.buff_to_draw != NULL)
    {
        for (uint32_t i = 0; i < npixels; i++)
        {
            *fb_ptr = lcd_RGB888toRGB565(rgb888);
            fb_ptr++;
        }

        ILI9341.write_length = ILI9341_PIXELS_TO_BYTES(npixels);

        while (!lcd_IsWriteAllowed());

        ILI9341.buff_to_flush = ILI9341.buff_to_draw;
        ILI9341.buff_to_draw = NULL;
        lcd_Flush();

        return ILI9341_PIXELS_TO_BYTES(npixels);
    }

    return 0;
}


static uint8_t lcd_IsWriteAllowed(void)
{
    uint8_t retVal;
    __disable_irq();
    retVal = (ILI9341.buff_to_flush) ? FALSE : TRUE;
    __enable_irq();
    return retVal;
}

static void lcd_Flush(void)
{
    lcd_SetDisplArea();
    /* Send command lcd memory write */
    lcd_WriteCmd(ILI9341_GRAM, NULL, 0);
    lcd_WriteDma((uint32_t) ILI9341.buff_to_flush, ILI9341.write_length);
}

static HAL_StatusTypeDef lcd_SPI_Send(SPI_HandleTypeDef *hspi, uint8_t *data, uint8_t len, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_ERROR;

    /* Check if the SPI is already enabled */
    if (!READ_BIT(hspi->Instance->CR1, SPI_CR1_SPE))
    {
        /* Enable SPI peripheral */
        __HAL_SPI_ENABLE(hspi);
    }

    while (len > 0U)
    {
        while (!__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE) && timeout--);

        if (timeout == 0U)
        {
            ret = HAL_TIMEOUT;
            break;
        }
        else
        {
//            if (READ_BIT(hspi->Instance->CR1, SPI_CR1_DFF))
        	if(ILI9341.hspi->Init.DataSize == SPI_DATASIZE_16BIT)
            {
                /* 16-bit mode */
                WRITE_REG(hspi->Instance->TXDR, *((uint16_t*) data++));
                len--;
                len--;
            }
            else
            {
                /* 8-bit mode */
                WRITE_REG(hspi->Instance->TXDR, *data++);
                len--;
            }

            ret = HAL_OK;
        }
    }

    while (__HAL_SPI_GET_FLAG(hspi, SPI_SR_EOT) && timeout--);
    __HAL_SPI_CLEAR_OVRFLAG(hspi);

    ret = (timeout == 0U) ? HAL_TIMEOUT: HAL_OK;

    return ret;
}


static HAL_StatusTypeDef lcd_SPI_SendRead(SPI_HandleTypeDef *hspi, uint8_t *rxData, uint8_t *txData, uint8_t len, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint32_t local_timeout;

    /* Enable SPI peripheral if not already enabled */
    if (!READ_BIT(hspi->Instance->CR1, SPI_CR1_SPE))
    {
        __HAL_SPI_ENABLE(hspi);
    }

    while (len > 0U)
    {
        local_timeout = timeout;

        /* Wait for TXE flag (ready to transmit) */
        while (!__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE) && local_timeout--);

        if (local_timeout == 0U)
        {
            ret = HAL_TIMEOUT;
            break;
        }

        /* Write data to SPI Data Register */
//        if (READ_BIT(hspi->Instance->CR1, SPI_CR1_DFF))
        if(ILI9341.hspi->Init.DataSize == SPI_DATASIZE_16BIT)
        {
            /* 16-bit mode */
            *((__IO uint16_t*) &hspi->Instance->TXDR) = *((uint16_t*) txData);
            txData += 2U;
            len -= 2U;
        }
        else
        {
            /* 8-bit mode */
            *((__IO uint8_t*) &hspi->Instance->TXDR) = *txData++;
            len--;
        }

        /* Wait for RXNE flag (data available) */
        local_timeout = timeout;
        while (!__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE) && local_timeout--);

        if (local_timeout == 0U)
        {
            ret = HAL_TIMEOUT;
            break;
        }

        /* Read received data */
//        if (READ_BIT(hspi->Instance->CR1, SPI_CR1_DFF))
        if(ILI9341.hspi->Init.DataSize == SPI_DATASIZE_16BIT)
        {
            *((uint16_t*) rxData) = *((__IO uint16_t*) &hspi->Instance->TXDR);
            rxData += 2U;
        }
        else
        {
            *rxData++ = *((__IO uint8_t*) &hspi->Instance->TXDR);
        }
    }

    /* Check BSY flag (SPI busy) */
    local_timeout = timeout;
    while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_EOT) && local_timeout--);

    if (local_timeout == 0U)
    {
        ret = HAL_TIMEOUT;
    }

    /* Clear any overrun flag that may have occurred */
    __HAL_SPI_CLEAR_OVRFLAG(hspi);

    return ret;
}



/******************************************************************************
 *                               HAL CALLBACKS                                *
 ******************************************************************************/

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    UNUSED(hspi);
    if (ILI9341.dma_err_cb != NULL)
    {
        ILI9341.dma_err_cb();
    }
    else
    {
        while (1);
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    //DEBUG_TIMEMEAS_START();
    uint32_t chunk_size, chunk_cnt, src_address, nitems, n_chunks;
    uint8_t needToCont;

    /* Read shared data */
    __disable_irq();
    src_address = ILI9341.spi_dma.src_address;
    nitems = ILI9341.spi_dma.nitems;
    chunk_cnt = ILI9341.spi_dma.chunk_cnt;
    n_chunks = ILI9341.spi_dma.n_chunks;
    __enable_irq();

    if (chunk_cnt == 0U)
    {
        /* All data chunks are already sent via DMA */
        ILI9341.buff_to_flush = NULL;

        /* Release CSX pin */
        ILI9341_CSX_HIGH();  // SPI CS

        /* Reset global counters */
        src_address = 0U;
        nitems = 0U;
        needToCont = FALSE;

//        DEBUG_MEAS_FPS(ILI9341.area.x1 == 0U && ILI9341.area.y1 == 1U);
    }
    else
    {
        /* Send next chunk of 16-bit data via DMA */
        if (nitems > ILI9341_DMA_MAX_ITEMS)
        {
            chunk_size = ILI9341_DMA_MAX_ITEMS;
            nitems -= ILI9341_DMA_MAX_ITEMS;
        }
        else
        {
            chunk_size = nitems;
        }

        /* Shift source address for the next data chunk */
        src_address += ILI9341_DMA_MAX_ITEMS * (n_chunks - chunk_cnt) * ILI9341_DATAWIDTH;
        chunk_cnt--;

        needToCont = TRUE; // needs to call HAL_SPI_Transmit_DMA()
    }

    /* Write shared data */
    __disable_irq();
    ILI9341.spi_dma.src_address = src_address;
    ILI9341.spi_dma.nitems = nitems;
    ILI9341.spi_dma.chunk_cnt = chunk_cnt;
    ILI9341.spi_dma.n_chunks = n_chunks;
    __enable_irq();

    if (ILI9341.dma_cplt_cb != NULL)
    {
        /* Call external callback */
        ILI9341.dma_cplt_cb(); // <- is not defined and not used in current application
    }

    if (needToCont == TRUE)
    {
        /* Draw next data chunk */
#if (ILI9341_USE_DMA_CMSIS == 1)
        spi_transmit_dma(ILI9341.hspi, (uint8_t*) ILI9341.spi_dma.src_address, chunk_size);
#else
        HAL_SPI_Transmit_DMA(ILI9341.hspi, (uint8_t*) ILI9341.spi_dma.src_address, chunk_size);
#endif
    }

    //DEBUG_TIMEMEAS_END();
}

#if (ILI9341_USE_DMA_CMSIS == 1)
static void spi_transmit_dma(SPI_HandleTypeDef *hspi, const uint8_t *pData, uint16_t Size)
{
    /* Configure DMA Stream data length */
    WRITE_REG(hspi->hdmatx->Instance->NDTR, Size);
    /* Configure DMA Stream source address */
    WRITE_REG(hspi->hdmatx->Instance->M0AR, (uint32_t)pData);
    /* Enable Common interrupts*/
    SET_BIT(hspi->hdmatx->Instance->CR, (DMA_IT_TC | DMA_IT_TE | DMA_IT_DME));
    /* Enable the Peripheral */
    SET_BIT(hspi->hdmatx->Instance->CR, DMA_SxCR_EN);
    /* Check if the SPI is already enabled */
    if (!READ_BIT(hspi->Instance->CR1, SPI_CR1_SPE))
    {
        /* Enable SPI peripheral */
        SET_BIT(hspi->Instance->CR1, SPI_CR1_SPE);
    }
    /* Enable the SPI Error Interrupt Bit */
    SET_BIT(hspi->Instance->CR2, SPI_IT_ERR);
    /* Enable Tx DMA Request */
    SET_BIT(hspi->Instance->CR2, SPI_CR2_TXDMAEN);
}

static void SPI_DMA_TxCpltCallback(DMA_HandleTypeDef *hdma)
{
    SPI_HandleTypeDef *hspi = (SPI_HandleTypeDef*)\
            (((DMA_HandleTypeDef*) hdma)->Parent);
    /* Disable ERR interrupt */
    __HAL_SPI_DISABLE_IT(hspi, SPI_IT_ERR);

    __HAL_DMA_CLEAR_FLAG(hspi->hdmatx, __HAL_DMA_GET_TC_FLAG_INDEX(hspi->hdmatx));
    __HAL_DMA_CLEAR_FLAG(hspi->hdmatx, __HAL_DMA_GET_HT_FLAG_INDEX(hspi->hdmatx));
    __HAL_DMA_CLEAR_FLAG(hspi->hdmatx, __HAL_DMA_GET_TE_FLAG_INDEX(hspi->hdmatx));
    __HAL_DMA_CLEAR_FLAG(hspi->hdmatx, __HAL_DMA_GET_DME_FLAG_INDEX(hspi->hdmatx));
    __HAL_DMA_CLEAR_FLAG(hspi->hdmatx, __HAL_DMA_GET_FE_FLAG_INDEX(hspi->hdmatx));
    /* Disable Tx DMA Request */
    CLEAR_BIT(hspi->Instance->CR2, SPI_CR2_TXDMAEN);
    /* Wait until TXE flag */
    while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE) != SET);
    /* Wait until BSY flag is RESET */
    while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_BSY) != RESET);
    HAL_SPI_TxCpltCallback(hspi);
}
#endif
