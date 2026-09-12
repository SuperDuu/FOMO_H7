/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "libjpeg.h"
#include "app_x-cube-ai.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "usbd_cdc_if.h"
#include "OV7670.h"
#include "LED.h"
#include "SD_Card.h"
#include "Button.h"
#include "ILI9341.h"
#include "ai_platform.h"
#include "network_1752296348456.h"
#include "network_1752296348456_data.h"
#include "golden_sample.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
//FATFS fs;  // File system object
FIL fil;   // File object
extern uint8_t retSD; // Return value
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ==============================================================================
 * APPLICATION OPERATION MODES:
 * 0 = APP_MODE_STATIC_TEST: Chạy nhận diện tự động liên tục trên ảnh mẫu 18_0005.jpg
 *                           Model STM32 tự suy luận, Parse_FOMO_Output tự tính tọa độ và vẽ target.
 * 1 = APP_MODE_LIVE_CAMERA: Chạy trực tiếp từ Camera OV7670
 * 2 = APP_MODE_DEMO_TIMED:  Chạy tự động ảnh 18_0005 trong 15s (đếm ngược), sau đó chuyển sang Camera
 * ============================================================================== */
#define APP_MODE_STATIC_TEST   0
#define APP_MODE_LIVE_CAMERA   1
#define APP_MODE_DEMO_TIMED    2

#define CURRENT_APP_MODE       APP_MODE_LIVE_CAMERA
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

CRC_HandleTypeDef hcrc;

DCMI_HandleTypeDef hdcmi;
DMA_HandleTypeDef hdma_dcmi;

DMA2D_HandleTypeDef hdma2d;

I2C_HandleTypeDef hi2c1;

SD_HandleTypeDef hsd1;

SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_tx;

TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim8;
TIM_HandleTypeDef htim13;

SRAM_HandleTypeDef hsram1;

/* USER CODE BEGIN PV */
extern volatile int8_t golden_raw_output[512];
extern volatile int8_t golden_peak_tgt;
extern volatile int8_t golden_peak_bg;
extern volatile int8_t golden_peak_x;
extern volatile int8_t golden_peak_y;
extern volatile int8_t golden_cell_7_8_tgt;
extern volatile int8_t golden_cell_7_8_bg;
extern volatile uint8_t is_golden_sample_running;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SDMMC1_SD_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM8_Init(void);
static void MX_TIM13_Init(void);
static void MX_DCMI_Init(void);
static void MX_I2C1_Init(void);
static void MX_CRC_Init(void);
static void MX_DMA2D_Init(void);
static void MX_FMC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _close(int file) { return -1; }
int _fstat(int file) { return -1; }
int _isatty(int file) { return 0; }
int _lseek(int file, int ptr, int dir) { return 0; }
int _read(int file, char *ptr, int len) { return 0; }
int _write(int file, char *ptr, int len) { return len; }
int _getpid(void) {return 1;}
int _kill(int pid, int sig) {return -1;}
 uint32_t t;
 int32_t delta_t;
 float f=0;
extern const uint8_t LOGO[];
extern const uint32_t LOGO_size;
AI_ALIGNED(32) __attribute__((section(".RAM_D2"))) ai_i8 data_in_1[AI_NETWORK_1752296348456_IN_1_SIZE_BYTES];
AI_ALIGNED(32) __attribute__((section(".RAM_D2"))) ai_i8 data_in_2[AI_NETWORK_1752296348456_IN_1_SIZE_BYTES];
int cnt = 0;
int ai_x[10], ai_y[10], ai_w[10], ai_h[10];
float ai_score[10];
uint32_t ai_time_ms = 0;
volatile int ai_detection_count = 0;
volatile float ai_peak_score = 0.0f;
volatile int8_t ai_peak_tgt = -128, ai_peak_bg = -128;
volatile int ai_peak_gx = 0, ai_peak_gy = 0;
volatile int ai_cells_over_bg = 0, ai_cells_over_conf = 0;
static char usb_log_buf[2][256];
static uint8_t usb_buf_idx = 0;
char label[32];
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
    uint32_t frameCount;
     uint32_t 			lastTick;
     float 				fps;

} OV7670;

uint8_t flag_ai_ready = 0;
volatile uint8_t flag_frame_ready = 0;
volatile uint8_t cam_buf_idx = 0;
volatile ai_i8* current_ai_buffer = data_in_1;
void Set_AI_Input_Buffer(ai_i8* buffer);

/* ============ Fast string helpers (no sprintf) ============ */
static inline int fast_itoa(int val, char *buf) {
	int pos = 0;
	if (val < 0) { buf[pos++] = '-'; val = -val; }
	if (val == 0) { buf[pos++] = '0'; return pos; }
	char tmp[10]; int len = 0;
	while (val > 0) { tmp[len++] = '0' + (val % 10); val /= 10; }
	for (int i = len - 1; i >= 0; i--) buf[pos++] = tmp[i];
	return pos;
}

static inline int fast_append_str(char *buf, int pos, const char *s) {
	while (*s) buf[pos++] = *s++;
	return pos;
}

/* Format: "FPS: XX.X | AI: NNms" */
static void build_fps_label(char *buf, int fps_x10, int ai_ms) {
	int p = 0;
	p = fast_append_str(buf, p, "FPS:");
	p += fast_itoa(fps_x10 / 10, buf + p);
	buf[p++] = '.';
	p += fast_itoa(fps_x10 % 10, buf + p);
	p = fast_append_str(buf, p, " | AI:");
	p += fast_itoa(ai_ms, buf + p);
	p = fast_append_str(buf, p, "ms");
	buf[p] = '\0';
}

/* Format: "Target XX% (X,Y)" */
static void build_fomo_label(char *buf, int score_pct, int cx, int cy) {
	int p = 0;
	p = fast_append_str(buf, p, "Target ");
	p += fast_itoa(score_pct, buf + p);
	buf[p++] = '%';
	buf[p++] = ' ';
	buf[p++] = '(';
	p += fast_itoa(cx, buf + p);
	buf[p++] = ',';
	p += fast_itoa(cy, buf + p);
	buf[p++] = ')';
	buf[p] = '\0';
}

static void Send_Telemetry_USB(const char *tag) {
	char *p_usb = usb_log_buf[usb_buf_idx];
	usb_buf_idx ^= 1;
	int n = snprintf(p_usb, 256,
			"[%s] #%d fps=%.1f dt=%lums dets=%d | peak=(%d,%d) sc=%.1f%% (t=%d,b=%d) over_bg=%d/256 over_th=%d |",
			tag, cnt, OV7670.fps, (unsigned long)ai_time_ms, ai_detection_count,
			ai_peak_gx, ai_peak_gy, ai_peak_score * 100.0f,
			(int)ai_peak_tgt, (int)ai_peak_bg,
			ai_cells_over_bg, ai_cells_over_conf);
	for (int i = 0; i < ai_detection_count && i < 4; i++) {
		if (n < 256 - 32) {
			n += snprintf(p_usb + n, 256 - n,
					" D%d:[%d,%d,%.1f%%]", i, ai_x[i], ai_y[i], ai_score[i] * 100.0f);
		}
	}
	if (n < 256 - 3) {
		p_usb[n++] = '\r';
		p_usb[n++] = '\n';
		p_usb[n] = '\0';
	}
	CDC_Transmit_FS((uint8_t*)p_usb, (uint16_t)n);
}

/* ============ Minimal ISR — only set flag + restart capture ============ */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
	if (hdcmi->Instance == OV7670.hdcmi->Instance) {
		/* 1. Stop DCMI DMA to switch buffer (XCLK TIM5 keeps running!) */
		HAL_DCMI_Stop(OV7670.hdcmi);

		uint8_t finished_idx = cam_buf_idx;
		cam_buf_idx ^= 1U;

		/* 2. Immediately restart DCMI DMA on the OTHER buffer so camera never waits */
		HAL_DCMI_Start_DMA(OV7670.hdcmi, DCMI_MODE_CONTINUOUS, OV7670.buffer_addr[cam_buf_idx], OV7670_FRAME_SIZE_WORDS);

		/* 3. Invalidate D-Cache so CPU reads fresh DMA data from the JUST FILLED buffer */
		SCB_InvalidateDCache_by_Addr((uint32_t*)OV7670.buffer_addr[finished_idx], OV7670_FRAME_SIZE_BYTES);

		/* 4. FPS tracking */
		uint32_t currentTick = HAL_GetTick();
		OV7670.frameCount++;
		if (currentTick - OV7670.lastTick >= 500) {
			OV7670.fps = (OV7670.frameCount * 1000.0f / (currentTick - OV7670.lastTick));
			OV7670.frameCount = 0;
			OV7670.lastTick = currentTick;
		}

		/* Signal main loop that a new frame is ready */
		flag_frame_ready = 1;

		/* Step 1: Crop and Convert to the FREE AI buffer (ping-pong) */
		ai_i8* target_buffer = (current_ai_buffer == data_in_1) ? data_in_2 : data_in_1;
		Crop_and_Convert_Fast((uint8_t*)OV7670.buffer_addr[finished_idx], (uint8_t*)target_buffer);

		/* Step 2: If AI is IDLE, give it the fresh buffer and trigger it */
		if (flag_ai_ready == 0) {
			current_ai_buffer = target_buffer;
			flag_ai_ready = 1;
		}

		/* Step 3: Send to LCD asynchronously ONLY if SPI DMA is completely free */
		if (!ILI9341_IsBusy()) {
			uint8_t *fb = (uint8_t*)OV7670.buffer_addr[finished_idx];

			/* Draw overlays directly into this finished framebuffer */
			Draw_Rectangle_Outline((ILI9341_ACTIVE_WIDTH - FOMO_CROP_SIZE) / 2,
								   (ILI9341_ACTIVE_HEIGHT - FOMO_CROP_SIZE) / 2,
								   FOMO_CROP_SIZE, FOMO_CROP_SIZE, 0x7BEF);

			int drawn_count = 0;
			for (int i = 0; i < 5; i++) {
				if (ai_score[i] < FOMO_CONF_THRESHOLD || ai_score[i] <= 0.0f) continue;
				Draw_Target_Marker(ai_x[i], ai_y[i], 16, 0x07E0);
				build_fomo_label(label, (int)(ai_score[i] * 100), ai_x[i], ai_y[i]);
				int label_x = (ai_x[i] >= 45) ? (ai_x[i] - 45) : 2;
				if (label_x > 180) label_x = 180;
				int label_y = (ai_y[i] >= 16) ? (ai_y[i] - 16) : (ai_y[i] + 12);
				LCD_PrintStringColor(label_x, label_y, label, 0x07E0);
				drawn_count++;
				if (drawn_count >= 4) break;
			}

			build_fps_label(label, (int)(OV7670.fps * 10), (int)ai_time_ms);
			LCD_PrintStringColor((320 - FOMO_CROP_SIZE) / 2, 2, label, 0x07E0);

			/* Clean D-Cache before SPI DMA reads the framebuffer */
			SCB_CleanDCache_by_Addr((uint32_t*)fb, OV7670_FRAME_SIZE_BYTES);

			/* Send frame to LCD via SPI DMA (non-blocking) */
			ILI9341_DrawFrame(fb, OV7670_FRAME_SIZE_BYTES);
		}
	}
}

/* ==============================================================================
 * Static Golden Sample Autonomous Detection Engine
 * ============================================================================== */
static void __attribute__((unused)) Render_Golden_Sample_To_Framebuffer(uint16_t *fb)
{
	/* Clear side borders to black (X: 0..39 and 280..319) */
	for (int y = 0; y < 240; y++) {
		for (int x = 0; x < 40; x++) fb[y * 320 + x] = 0x0000;
		for (int x = 280; x < 320; x++) fb[y * 320 + x] = 0x0000;
	}

	/* Scale 128x128 image to 240x240 inside X:[40..279], Y:[0..239] */
	for (int y = 0; y < 240; y++) {
		int img_y = (y * 128) / 240;
		int row_offset = img_y * 128;
		int fb_row = y * 320 + 40;
		for (int x = 0; x < 240; x++) {
			int img_x = (x * 128) / 240;
			int idx = (row_offset + img_x) * 3;
			uint8_t r = (uint8_t)((int16_t)golden_sample_18_0005[idx] + 128);
			uint8_t g = (uint8_t)((int16_t)golden_sample_18_0005[idx + 1] + 128);
			uint8_t b = (uint8_t)((int16_t)golden_sample_18_0005[idx + 2] + 128);
			fb[fb_row + x] = (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
		}
	}
}

static void __attribute__((unused)) Process_Static_Test_Frame(uint32_t remaining_ms)
{
	uint16_t *fb = (uint16_t*)OV7670.buffer_addr[0];

	/* Wait for any previous SPI DMA frame to finish */
	uint32_t t_wait = HAL_GetTick();
	while (ILI9341_IsBusy() && (HAL_GetTick() - t_wait < 100));

	/* 1. Render test image onto Framebuffer */
	Render_Golden_Sample_To_Framebuffer(fb);

	/* 2. Draw 240x240 crop boundary box */
	Draw_Rectangle_Outline(40, 0, 240, 240, 0x7BEF);

	/* 3. Run live AI inference on golden sample */
	Set_AI_Input_Buffer((ai_i8*)golden_sample_18_0005);
	uint32_t t_start = HAL_GetTick();
	MX_X_CUBE_AI_Process(); /* Invokes post_process() -> Parse_FOMO_Output() */
	ai_time_ms = HAL_GetTick() - t_start;
	cnt++;
	Send_Telemetry_USB("TEST");

	/* 4. Draw detections generated entirely by Parse_FOMO_Output() */
	int drawn_count = 0;
	for (int i = 0; i < 5; i++) {
		if (ai_score[i] < FOMO_CONF_THRESHOLD || ai_score[i] <= 0.0f) continue;

		/* Crosshair marker at detected center */
		Draw_Target_Marker(ai_x[i], ai_y[i], 16, 0x07E0);

		build_fomo_label(label, (int)(ai_score[i] * 100), ai_x[i], ai_y[i]);
		int label_x = (ai_x[i] >= 45) ? (ai_x[i] - 45) : 2;
		if (label_x > 180) label_x = 180;
		int label_y = (ai_y[i] >= 16) ? (ai_y[i] - 16) : (ai_y[i] + 12);
		LCD_PrintStringColor(label_x, label_y, label, 0x07E0);
		drawn_count++;
		if (drawn_count >= 4) break;
	}

	/* 5. Status text overlays on left margin (X: 2..38) */
	char status_buf[48];
	LCD_PrintStringColor(2, 4, "TEST", 0xFFE0);
	LCD_PrintStringColor(2, 20, "18_05", 0xFFFF);

	snprintf(status_buf, sizeof(status_buf), "Obj:%d", drawn_count);
	LCD_PrintStringColor(2, 40, status_buf, (drawn_count > 0) ? 0x07E0 : 0xF800);

	if (drawn_count > 0) {
		snprintf(status_buf, sizeof(status_buf), "%d%%", (int)(ai_score[0] * 100));
		LCD_PrintStringColor(2, 56, status_buf, 0x07E0);
	}

	snprintf(status_buf, sizeof(status_buf), "%lums", (unsigned long)ai_time_ms);
	LCD_PrintStringColor(2, 76, status_buf, 0x7BEF);

	/* Status on right margin (X: 282..318) */
	if (drawn_count > 0) {
		snprintf(status_buf, sizeof(status_buf), "X:%d", ai_x[0]);
		LCD_PrintStringColor(282, 40, status_buf, 0x07E0);
		snprintf(status_buf, sizeof(status_buf), "Y:%d", ai_y[0]);
		LCD_PrintStringColor(282, 56, status_buf, 0x07E0);
	}
	snprintf(status_buf, sizeof(status_buf), "#%d", cnt % 1000);
	LCD_PrintStringColor(282, 76, status_buf, 0x7BEF);

	if (remaining_ms > 0) {
		snprintf(status_buf, sizeof(status_buf), "Cam:%lus", (unsigned long)(remaining_ms / 1000 + 1));
		LCD_PrintStringColor(2, 220, status_buf, 0xFD20);
	} else {
		LCD_PrintStringColor(2, 220, "LIVE", 0x07E0);
	}

	/* 6. Clean D-Cache and flush to ILI9341 display */
	SCB_CleanDCache_by_Addr((uint32_t*)OV7670.buffer_addr[0], OV7670_FRAME_SIZE_BYTES);
	ILI9341_DrawFrame((uint8_t*)OV7670.buffer_addr[0], OV7670_FRAME_SIZE_BYTES);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SDMMC1_SD_Init();
  MX_SPI2_Init();
  MX_TIM5_Init();
  MX_TIM8_Init();
  MX_TIM13_Init();
  MX_DCMI_Init();
  MX_FATFS_Init();
  MX_I2C1_Init();
  MX_LIBJPEG_Init();
  MX_CRC_Init();
  MX_DMA2D_Init();
  MX_FMC_Init();
  MX_USB_DEVICE_Init();
  MX_X_CUBE_AI_Init();
  /* USER CODE BEGIN 2 */
  ILI9341_Init(&hspi2, ILI9341_PIXEL_FMT_RGB565);
//  ILI9341_SetBackgroundColor(BLACK);
  OV7670_Init(&hdcmi, &hi2c1, &htim5, TIM_CHANNEL_3);
  ILI9341_DrawFrame(LOGO, LOGO_size);
  HAL_Delay(1000);

#if (CURRENT_APP_MODE == APP_MODE_DEMO_TIMED)
  uint32_t demo_start = HAL_GetTick();
  while (HAL_GetTick() - demo_start < 15000) {
    uint32_t rem = 15000 - (HAL_GetTick() - demo_start);
    Process_Static_Test_Frame(rem);
  }
  OV7670_Start();
#elif (CURRENT_APP_MODE == APP_MODE_LIVE_CAMERA)
  OV7670_Start();
#elif (CURRENT_APP_MODE == APP_MODE_STATIC_TEST)
  /* Camera not started; Process_Static_Test_Frame runs in while(1) */
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

//  MX_X_CUBE_AI_Process(); /* Spurious CubeMX call removed: inference only on flag_ai_ready */
    /* USER CODE BEGIN 3 */
#if (CURRENT_APP_MODE == APP_MODE_STATIC_TEST)
	  Process_Static_Test_Frame(0);
#else
	  /* ====== Frame processing pipeline (runs in main context, NOT ISR) ====== */
	  if (flag_frame_ready) {
		  flag_frame_ready = 0;
	  }

	  /* AI inference — only when a new frame has been preprocessed */
	  if (flag_ai_ready) {
		  uint32_t t_start = HAL_GetTick();
		  Set_AI_Input_Buffer((ai_i8*)current_ai_buffer);
		  MX_X_CUBE_AI_Process();
		  ai_time_ms = HAL_GetTick() - t_start;
		  flag_ai_ready = 0;
		  cnt++;
		  Send_Telemetry_USB("CAM");
	  }
#endif
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief DCMI Initialization Function
  * @param None
  * @retval None
  */
static void MX_DCMI_Init(void)
{

  /* USER CODE BEGIN DCMI_Init 0 */

  /* USER CODE END DCMI_Init 0 */

  /* USER CODE BEGIN DCMI_Init 1 */

  /* USER CODE END DCMI_Init 1 */
  hdcmi.Instance = DCMI;
  hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
  hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_RISING;
  hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_HIGH;
  hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;
  hdcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;
  hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
  hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;
  hdcmi.Init.ByteSelectMode = DCMI_BSM_ALL;
  hdcmi.Init.ByteSelectStart = DCMI_OEBS_ODD;
  hdcmi.Init.LineSelectMode = DCMI_LSM_ALL;
  hdcmi.Init.LineSelectStart = DCMI_OELS_ODD;
  if (HAL_DCMI_Init(&hdcmi) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DCMI_Init 2 */

  /* USER CODE END DCMI_Init 2 */

}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_R2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = 0;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */
  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x307075B1;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SDMMC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDMMC1_SD_Init(void)
{

  /* USER CODE BEGIN SDMMC1_Init 0 */

  /* USER CODE END SDMMC1_Init 0 */

  /* USER CODE BEGIN SDMMC1_Init 1 */

  /* USER CODE END SDMMC1_Init 1 */
  hsd1.Instance = SDMMC1;
  hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsd1.Init.BusWide = SDMMC_BUS_WIDE_4B;
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd1.Init.ClockDiv = 0;
  /* USER CODE BEGIN SDMMC1_Init 2 */

  /* USER CODE END SDMMC1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 0x0;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi2.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi2.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 3;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_OC_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TOGGLE;
  sConfigOC.Pulse = 1;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */
  HAL_TIM_MspPostInit(&htim5);

}

/**
  * @brief TIM8 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM8_Init(void)
{

  /* USER CODE BEGIN TIM8_Init 0 */

  /* USER CODE END TIM8_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM8_Init 1 */

  /* USER CODE END TIM8_Init 1 */
  htim8.Instance = TIM8;
  htim8.Init.Prescaler = 2399;
  htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim8.Init.Period = 999;
  htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_OC_Init(&htim8) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TOGGLE;
  sConfigOC.Pulse = 500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_OC_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim8, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM8_Init 2 */

  /* USER CODE END TIM8_Init 2 */
  HAL_TIM_MspPostInit(&htim8);

}

/**
  * @brief TIM13 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM13_Init(void)
{

  /* USER CODE BEGIN TIM13_Init 0 */

  /* USER CODE END TIM13_Init 0 */

  /* USER CODE BEGIN TIM13_Init 1 */

  /* USER CODE END TIM13_Init 1 */
  htim13.Instance = TIM13;
  htim13.Init.Prescaler = 2399;
  htim13.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim13.Init.Period = 99;
  htim13.Init.ClockDivision = TIM_CLOCKDIVISION_DIV2;
  htim13.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim13) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM13_Init 2 */

  /* USER CODE END TIM13_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

}

/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_NORSRAM_TimingTypeDef Timing = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SRAM1 memory initialization sequence
  */
  hsram1.Instance = FMC_NORSRAM_DEVICE;
  hsram1.Extended = FMC_NORSRAM_EXTENDED_DEVICE;
  /* hsram1.Init */
  hsram1.Init.NSBank = FMC_NORSRAM_BANK1;
  hsram1.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_ENABLE;
  hsram1.Init.MemoryType = FMC_MEMORY_TYPE_PSRAM;
  hsram1.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_16;
  hsram1.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;
  hsram1.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram1.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;
  hsram1.Init.WriteOperation = FMC_WRITE_OPERATION_DISABLE;
  hsram1.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;
  hsram1.Init.ExtendedMode = FMC_EXTENDED_MODE_DISABLE;
  hsram1.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram1.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;
  hsram1.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;
  hsram1.Init.WriteFifo = FMC_WRITE_FIFO_ENABLE;
  hsram1.Init.PageSize = FMC_PAGE_SIZE_NONE;
  /* Timing */
  Timing.AddressSetupTime = 15;
  Timing.AddressHoldTime = 15;
  Timing.DataSetupTime = 255;
  Timing.BusTurnAroundDuration = 15;
  Timing.CLKDivision = 16;
  Timing.DataLatency = 17;
  Timing.AccessMode = FMC_ACCESS_MODE_A;
  /* ExtTiming */

  if (HAL_SRAM_Init(&hsram1, &Timing, NULL) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */

  /* USER CODE END FMC_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_CSX_Pin|LCD_DCX_Pin|LCD_RESX_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, CAM_RET_Pin|CAM_PWDN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : SDIO_DETECT_Pin */
  GPIO_InitStruct.Pin = SDIO_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(SDIO_DETECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_CSX_Pin */
  GPIO_InitStruct.Pin = LCD_CSX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_CSX_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_DCX_Pin LCD_RESX_Pin */
  GPIO_InitStruct.Pin = LCD_DCX_Pin|LCD_RESX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : CAM_RET_Pin CAM_PWDN_Pin */
  GPIO_InitStruct.Pin = CAM_RET_Pin|CAM_PWDN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
