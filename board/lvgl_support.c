/*
 * Copyright 2019-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lvgl_support.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/misc/lv_profiler_builtin_private.h"
#if defined(SDK_OS_FREE_RTOS)
#include "FreeRTOS.h"
#include "semphr.h"
#endif
#include "board.h"

#include "fsl_cache.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"

#include "fsl_gt911.h"

#if 1 // LV_USE_GPU_NXP_VG_LITE
#include "vg_lite.h"
#include "vglite_support.h"
#endif

#if LV_USE_GPU_NXP_PXP
#include "draw/nxp/pxp/lv_draw_pxp_blend.h"
#endif

#if (DEMO_DISPLAY_CONTROLLER == DEMO_DISPLAY_CONTROLLER_LCDIFV2)
#include "fsl_lcdifv2.h"
#else
#include "fsl_elcdif.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Ratate panel or not. */
#ifndef DEMO_USE_ROTATE
#if LV_USE_GPU_NXP_PXP
#define DEMO_USE_ROTATE 1
#else
#define DEMO_USE_ROTATE 0
#endif
#endif

/* Cache line size. */
#ifndef FSL_FEATURE_L2CACHE_LINESIZE_BYTE
#define FSL_FEATURE_L2CACHE_LINESIZE_BYTE 0
#endif
#ifndef FSL_FEATURE_L1DCACHE_LINESIZE_BYTE
#define FSL_FEATURE_L1DCACHE_LINESIZE_BYTE 0
#endif

#if (FSL_FEATURE_L2CACHE_LINESIZE_BYTE > FSL_FEATURE_L1DCACHE_LINESIZE_BYTE)
#define DEMO_CACHE_LINE_SIZE FSL_FEATURE_L2CACHE_LINESIZE_BYTE
#else
#define DEMO_CACHE_LINE_SIZE FSL_FEATURE_L1DCACHE_LINESIZE_BYTE
#endif

#if (DEMO_CACHE_LINE_SIZE > FRAME_BUFFER_ALIGN)
#define DEMO_FB_ALIGN DEMO_CACHE_LINE_SIZE
#else
#define DEMO_FB_ALIGN FRAME_BUFFER_ALIGN
#endif

#if (LV_ATTRIBUTE_MEM_ALIGN_SIZE > DEMO_FB_ALIGN)
#undef DEMO_FB_ALIGN
#define DEMO_FB_ALIGN LV_ATTRIBUTE_MEM_ALIGN_SIZE
#endif

#define DEMO_FB_SIZE \
    (((DEMO_BUFFER_WIDTH * DEMO_BUFFER_HEIGHT * LCD_FB_BYTE_PER_PIXEL) + DEMO_FB_ALIGN - 1) & ~(DEMO_FB_ALIGN - 1))
#define DEMO_FB_USE_NONCACHEABLE_SECTION

#if DEMO_USE_ROTATE
#define LVGL_BUFFER_WIDTH DEMO_BUFFER_HEIGHT
#define LVGL_BUFFER_HEIGHT DEMO_BUFFER_WIDTH
#else
#define LVGL_BUFFER_WIDTH DEMO_BUFFER_WIDTH
#define LVGL_BUFFER_HEIGHT DEMO_BUFFER_HEIGHT
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static void DEMO_InitTouch(void);

static void DEMO_BufferSwitchOffCallback(void* param, void* switchOffBuffer);

static void BOARD_PullMIPIPanelTouchResetPin(bool pullUp);

static void BOARD_ConfigMIPIPanelTouchIntPin(gt911_int_pin_mode_t mode);

static void DEMO_WaitBufferSwitchOff(void);

#if ((LV_COLOR_DEPTH == 8) || (LV_COLOR_DEPTH == 1))
/*
 * To support 8 color depth and 1 color depth with this board, color palette is
 * used to map 256 color to 2^16 color.
 */
static void DEMO_SetLcdColorPalette(void);
#endif

/*******************************************************************************
 * Variables
 ******************************************************************************/
#ifdef DEMO_FB_USE_NONCACHEABLE_SECTION
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_frameBuffer[2][DEMO_FB_SIZE], DEMO_FB_ALIGN);
#else
SDK_ALIGN(static uint8_t s_frameBuffer[2][DEMO_FB_SIZE], DEMO_FB_ALIGN);
#endif

#if DEMO_USE_ROTATE
SDK_ALIGN(static uint8_t s_lvglBuffer[1][DEMO_FB_SIZE], DEMO_FB_ALIGN);
#endif

#if __CORTEX_M == 4
#define DEMO_FLUSH_DCACHE() L1CACHE_CleanInvalidateSystemCache()
#else
#if DEMO_USE_ROTATE
#define DEMO_FLUSH_DCACHE() SCB_CleanInvalidateDCache_by_Addr(s_lvglBuffer[0], DEMO_FB_SIZE)
#else
#define DEMO_FLUSH_DCACHE() SCB_CleanInvalidateDCache()
#endif
#endif

#if defined(SDK_OS_FREE_RTOS)
static SemaphoreHandle_t s_transferDone;
#else
static volatile bool s_transferDone;
#endif

#if DEMO_USE_ROTATE
/*
 * When rotate is used, LVGL stack draws in one buffer (s_lvglBuffer), and LCD
 * driver uses two buffers (s_frameBuffer) to remove tearing effect.
 */
static void* volatile s_inactiveFrameBuffer;
#endif

static gt911_handle_t s_touchHandle;
static const gt911_config_t s_touchConfig = {
    .I2C_SendFunc = BOARD_MIPIPanelTouch_I2C_Send,
    .I2C_ReceiveFunc = BOARD_MIPIPanelTouch_I2C_Receive,
    .pullResetPinFunc = BOARD_PullMIPIPanelTouchResetPin,
    .intPinFunc = BOARD_ConfigMIPIPanelTouchIntPin,
    .timeDelayMsFunc = VIDEO_DelayMs,
    .touchPointNum = 1,
    .i2cAddrMode = kGT911_I2cAddrMode0,
    .intTrigMode = kGT911_IntRisingEdge,
};
static int s_touchResolutionX;
static int s_touchResolutionY;

/*******************************************************************************
 * Code
 ******************************************************************************/

#if ((LV_COLOR_DEPTH == 8) || (LV_COLOR_DEPTH == 1))
static void DEMO_SetLcdColorPalette(void)
{
    /*
     * To support 8 color depth and 1 color depth with this board, color palette is
     * used to map 256 color to 2^16 color.
     *
     * LVGL 1-bit color depth still uses 8-bit per pixel, so the palette size is the
     * same with 8-bit color depth.
     */
    uint32_t palette[256];

#if (LV_COLOR_DEPTH == 8)
    lv_color_t color;
    color.full = 0U;

    /* RGB332 map to RGB888 */
    for (int i = 0; i < 256U; i++) {
        palette[i] = ((uint32_t)color.ch.blue << 6U) | ((uint32_t)color.ch.green << 13U) | ((uint32_t)color.ch.red << 21U);
        color.full++;
    }

#elif (LV_COLOR_DEPTH == 1)
    for (int i = 0; i < 256U;) {
        /*
         * Pixel map:
         * 0bXXXXXXX1 -> 0xFFFFFF
         * 0bXXXXXXX0 -> 0x000000
         */
        palette[i] = 0x000000U;
        i++;
        palette[i] = 0xFFFFFFU;
        i++;
    }
#endif

#if (DEMO_DISPLAY_CONTROLLER == DEMO_DISPLAY_CONTROLLER_ELCDIF)
    ELCDIF_UpdateLut(LCDIF, kELCDIF_Lut0, 0, palette, 256);
    ELCDIF_EnableLut(LCDIF, true);
#else
    LCDIFV2_SetLut(LCDIFV2, 0, palette, 256, false);
#endif
}
#endif

void lv_port_pre_init(void)
{
}

#ifndef DISABLE_DISPLAY

static void DEMO_BufferSwitchOffCallback(void* param, void* switchOffBuffer)
{
#if defined(SDK_OS_FREE_RTOS)
    BaseType_t taskAwake = pdFALSE;

    xSemaphoreGiveFromISR(s_transferDone, &taskAwake);
    portYIELD_FROM_ISR(taskAwake);
#else
    s_transferDone = true;
#endif

#if DEMO_USE_ROTATE
    s_inactiveFrameBuffer = switchOffBuffer;
#endif
}

static void DEMO_WaitBufferSwitchOff(void)
{
#if defined(SDK_OS_FREE_RTOS)
    if (xSemaphoreTake(s_transferDone, portMAX_DELAY) != pdTRUE) {
        PRINTF("Display flush failed\r\n");
        assert(0);
    }
#else
    while (false == s_transferDone) {
    }
    s_transferDone = false;
#endif
}

static void DEMO_FlushDisplay(lv_display_t* disp, const lv_area_t* area, uint8_t* color_p)
{
#if DEMO_USE_ROTATE

    /*
     * Work flow:
     *
     * 1. Wait for the available inactive frame buffer to draw.
     * 2. Draw the ratated frame to inactive buffer.
     * 3. Pass inactive to LCD controller to show.
     */

    static bool firstFlush = true;

    /* Only wait for the first time. */
    if (firstFlush) {
        firstFlush = false;
    } else {
        /* Wait frame buffer. */
        DEMO_WaitBufferSwitchOff();
    }

    /* Copy buffer. */
    void* inactiveFrameBuffer = s_inactiveFrameBuffer;

#if __CORTEX_M == 4
    L1CACHE_CleanInvalidateSystemCacheByRange((uint32_t)s_inactiveFrameBuffer, DEMO_FB_SIZE);
#else
    SCB_CleanInvalidateDCache_by_Addr(inactiveFrameBuffer, DEMO_FB_SIZE);
#endif

#if LV_USE_GPU_NXP_PXP /* Use PXP to rotate the panel. */
    lv_area_t dest_area = {
        .x1 = 0,
        .x2 = DEMO_BUFFER_HEIGHT - 1,
        .y1 = 0,
        .y2 = DEMO_BUFFER_WIDTH - 1,
    };

    lv_gpu_nxp_pxp_blit(((lv_color_t*)inactiveFrameBuffer), &dest_area, DEMO_BUFFER_WIDTH, color_p, area,
        lv_area_get_width(area), LV_OPA_COVER, LV_DISP_ROT_270);
    lv_gpu_nxp_pxp_wait();

#else /* Use CPU to rotate the panel. */
    for (uint32_t y = 0; y < LVGL_BUFFER_HEIGHT; y++) {
        for (uint32_t x = 0; x < LVGL_BUFFER_WIDTH; x++) {
            ((lv_color_t*)inactiveFrameBuffer)[(DEMO_BUFFER_HEIGHT - x) * DEMO_BUFFER_WIDTH + y] = color_p[y * LVGL_BUFFER_WIDTH + x];
        }
    }
#endif

#if __CORTEX_M == 4
    L1CACHE_CleanInvalidateSystemCacheByRange((uint32_t)s_inactiveFrameBuffer, DEMO_FB_SIZE);
#else
    SCB_CleanInvalidateDCache_by_Addr(inactiveFrameBuffer, DEMO_FB_SIZE);
#endif

    g_dc.ops->setFrameBuffer(&g_dc, 0, inactiveFrameBuffer);

    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);

#else /* DEMO_USE_ROTATE */

#ifndef DEMO_FB_USE_NONCACHEABLE_SECTION
    LV_PROFILER_BEGIN_TAG("SCB_CleanInvalidateDCache_by_Addr");
#if __CORTEX_M == 4
    L1CACHE_CleanInvalidateSystemCacheByRange((uint32_t)color_p, DEMO_FB_SIZE);
#else
    SCB_CleanInvalidateDCache_by_Addr(color_p, DEMO_FB_SIZE);
#endif
    LV_PROFILER_END_TAG("SCB_CleanInvalidateDCache_by_Addr");
#endif

    g_dc.ops->setFrameBuffer(&g_dc, 0, (void*)color_p);

    DEMO_WaitBufferSwitchOff();
#endif /* DEMO_USE_ROTATE */
}
#endif

static void disp_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* color_p)
{
#ifndef DISABLE_DISPLAY
    /* Skip the non-last flush */
    if (lv_display_flush_is_last(disp)) {
        DEMO_FlushDisplay(disp, area, color_p);
    }
#endif
    lv_display_flush_ready(disp);
}

void gpu_init(void)
{
    BOARD_PrepareVGLiteController();

    if (vg_lite_init(DEFAULT_VG_LITE_TW_WIDTH, DEFAULT_VG_LITE_TW_HEIGHT) != VG_LITE_SUCCESS) {
        PRINTF("VGLite init error. STOP.");
        vg_lite_close();
        while (1)
            ;
    }

    if (vg_lite_set_command_buffer_size(VG_LITE_COMMAND_BUFFER_SIZE) != VG_LITE_SUCCESS) {
        PRINTF("VGLite set command buffer. STOP.");
        vg_lite_close();
        while (1)
            ;
    }
}

void lv_port_disp_init(void)
{
    lv_display_t* disp = lv_display_create(LVGL_BUFFER_WIDTH, LVGL_BUFFER_HEIGHT);
    lv_display_set_flush_cb(disp, disp_flush_cb);

    lv_color_format_t color_format = DEMO_USE_XRGB8888 ? LV_COLOR_FORMAT_XRGB8888 : LV_COLOR_FORMAT_RGB565;

    static lv_draw_buf_t draw_buf_1;
    lv_draw_buf_init(&draw_buf_1,
        LVGL_BUFFER_WIDTH,
        LVGL_BUFFER_HEIGHT,
        color_format,
        LVGL_BUFFER_WIDTH * DEMO_BUFFER_BYTE_PER_PIXEL,
        s_frameBuffer[0],
        sizeof(s_frameBuffer[0]));

    static lv_draw_buf_t draw_buf_2;
    lv_draw_buf_init(&draw_buf_2,
        LVGL_BUFFER_WIDTH,
        LVGL_BUFFER_HEIGHT,
        color_format,
        LVGL_BUFFER_WIDTH * DEMO_BUFFER_BYTE_PER_PIXEL,
        s_frameBuffer[1],
        sizeof(s_frameBuffer[1]));

    lv_display_set_draw_buffers(disp, &draw_buf_1, &draw_buf_2);
    lv_display_set_color_format(disp, color_format);
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_DIRECT);

#if LV_USE_DRAW_VGLITE
    gpu_init();
#endif

#ifndef DISABLE_DISPLAY
    status_t status;
    dc_fb_info_t fbInfo;

    /*-------------------------
     * Initialize your display
     * -----------------------*/
    BOARD_PrepareDisplayController();

    status = g_dc.ops->init(&g_dc);
    if (kStatus_Success != status) {
        assert(0);
    }

#if ((LV_COLOR_DEPTH == 8) || (LV_COLOR_DEPTH == 1))
    DEMO_SetLcdColorPalette();
#endif

    g_dc.ops->getLayerDefaultConfig(&g_dc, 0, &fbInfo);
    fbInfo.pixelFormat = DEMO_BUFFER_PIXEL_FORMAT;
    fbInfo.width = DEMO_BUFFER_WIDTH;
    fbInfo.height = DEMO_BUFFER_HEIGHT;
    fbInfo.startX = DEMO_BUFFER_START_X;
    fbInfo.startY = DEMO_BUFFER_START_Y;
    fbInfo.strideBytes = DEMO_BUFFER_STRIDE_BYTE;
    g_dc.ops->setLayerConfig(&g_dc, 0, &fbInfo);

    g_dc.ops->setCallback(&g_dc, 0, DEMO_BufferSwitchOffCallback, NULL);

#if defined(SDK_OS_FREE_RTOS)
    s_transferDone = xSemaphoreCreateBinary();
    if (NULL == s_transferDone) {
        PRINTF("Frame semaphore create failed\r\n");
        assert(0);
    }
#else
    s_transferDone = false;
#endif

#if DEMO_USE_ROTATE
    /* s_frameBuffer[1] is first shown in the panel, s_frameBuffer[0] is inactive. */
    s_inactiveFrameBuffer = (void*)s_frameBuffer[0];
#endif

    /* lvgl starts render in frame buffer 0, so show frame buffer 1 first. */
    g_dc.ops->setFrameBuffer(&g_dc, 0, (void*)s_frameBuffer[1]);

    /* Wait for frame buffer sent to display controller video memory. */
    if ((g_dc.ops->getProperty(&g_dc) & kDC_FB_ReserveFrameBuffer) == 0) {
        DEMO_WaitBufferSwitchOff();
    }

    g_dc.ops->enableLayer(&g_dc, 0);
#endif
}

static void BOARD_PullMIPIPanelTouchResetPin(bool pullUp)
{
    if (pullUp) {
        GPIO_PinWrite(BOARD_MIPI_PANEL_TOUCH_RST_GPIO, BOARD_MIPI_PANEL_TOUCH_RST_PIN, 1);
    } else {
        GPIO_PinWrite(BOARD_MIPI_PANEL_TOUCH_RST_GPIO, BOARD_MIPI_PANEL_TOUCH_RST_PIN, 0);
    }
}

static void BOARD_ConfigMIPIPanelTouchIntPin(gt911_int_pin_mode_t mode)
{
    if (mode == kGT911_IntPinInput) {
        BOARD_MIPI_PANEL_TOUCH_INT_GPIO->GDIR &= ~(1UL << BOARD_MIPI_PANEL_TOUCH_INT_PIN);
    } else {
        if (mode == kGT911_IntPinPullDown) {
            GPIO_PinWrite(BOARD_MIPI_PANEL_TOUCH_INT_GPIO, BOARD_MIPI_PANEL_TOUCH_INT_PIN, 0);
        } else {
            GPIO_PinWrite(BOARD_MIPI_PANEL_TOUCH_INT_GPIO, BOARD_MIPI_PANEL_TOUCH_INT_PIN, 1);
        }

        BOARD_MIPI_PANEL_TOUCH_INT_GPIO->GDIR |= (1UL << BOARD_MIPI_PANEL_TOUCH_INT_PIN);
    }
}

/*Initialize your touchpad*/
static void DEMO_InitTouch(void)
{
    status_t status;

    const gpio_pin_config_t resetPinConfig = {
        .direction = kGPIO_DigitalOutput, .outputLogic = 0, .interruptMode = kGPIO_NoIntmode
    };
    GPIO_PinInit(BOARD_MIPI_PANEL_TOUCH_INT_GPIO, BOARD_MIPI_PANEL_TOUCH_INT_PIN, &resetPinConfig);
    GPIO_PinInit(BOARD_MIPI_PANEL_TOUCH_RST_GPIO, BOARD_MIPI_PANEL_TOUCH_RST_PIN, &resetPinConfig);

    status = GT911_Init(&s_touchHandle, &s_touchConfig);

    if (kStatus_Success != status) {
        PRINTF("Touch IC initialization failed\r\n");
        assert(false);
    }

    GT911_GetResolution(&s_touchHandle, &s_touchResolutionX, &s_touchResolutionY);
}

#ifndef DISABLE_TOUCH

/* Will be called by the library to read the touchpad */
static void DEMO_ReadTouch(lv_indev_t* indev, lv_indev_data_t* data)
{
    static int touch_x = 0;
    static int touch_y = 0;

    if (kStatus_Success == GT911_GetSingleTouch(&s_touchHandle, &touch_x, &touch_y)) {
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
#if DEMO_USE_ROTATE
    data->point.x = DEMO_PANEL_HEIGHT - (touch_y * DEMO_PANEL_HEIGHT / s_touchResolutionY);
    data->point.y = touch_x * DEMO_PANEL_WIDTH / s_touchResolutionX;
#else
    data->point.x = touch_x * DEMO_PANEL_WIDTH / s_touchResolutionX;
    data->point.y = touch_y * DEMO_PANEL_HEIGHT / s_touchResolutionY;
#endif
}

#endif

void lv_port_indev_init(void)
{
#ifndef DISABLE_TOUCH
    lv_indev_t* indev = lv_indev_create();

    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad */
    DEMO_InitTouch();

    /*Register a touchpad input device*/
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, DEMO_ReadTouch);
#endif
}

#include "core_cm7.h"
#include <stdio.h>

#define DWT_CYCLE_CNT (DWT->CYCCNT)
static void DWT_Init(void)
{
    /* Enable ITM and DWT resources, if not left enabled by debugger. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Make sure the high speed cycle counter is running.  It will be started
     * automatically only if a debugger is connected.
     */
    ITM->LAR = 0xC5ACCE55;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t g_cpu_freq = 0;
#define TICK_TO_NSEC(tick) ((tick) * 1000 / g_cpu_freq)

static uint64_t tick_get_cb(void)
{
    static uint32_t prev_tick = 0;
    static uint64_t cur_tick_ns = 0;
    uint32_t act_time = DWT_CYCLE_CNT;
    uint64_t elaps;

    /*If there is no overflow in sys_time simple subtract*/
    if (act_time >= prev_tick) {
        elaps = act_time - prev_tick;
    } else {
        elaps = UINT32_MAX - prev_tick + 1;
        elaps += act_time;
    }

    cur_tick_ns += TICK_TO_NSEC(elaps);
    prev_tick = act_time;
    return cur_tick_ns;
}

static int tid_get_cb(void)
{
    return (int)xTaskGetCurrentTaskHandle();
}

static void profiler_flush_cb(const char* buf)
{
    printf("%s", buf);
}

void lv_port_profiler_init(void)
{
#if LV_USE_PROFILER
    g_cpu_freq = 1000;
    DWT_Init();

    LV_LOG_USER("CPU frequency: %" LV_PRIu32 " MHz", g_cpu_freq);

    lv_profiler_builtin_config_t config;
    lv_profiler_builtin_config_init(&config);
    config.tick_per_sec = 1000000000; /* 1 sec = 1000000000 nsec */
    config.tick_get_cb = tick_get_cb;
    config.tid_get_cb = tid_get_cb;
    config.flush_cb = profiler_flush_cb;
    lv_profiler_builtin_init(&config);
#endif
}

#include "lvgl/src/stdlib/builtin/lv_tlsf.h"
#include "lvgl/src/draw/lv_draw_buf_private.h"

static lv_tlsf_t image_tlsf;

static void * buf_malloc_cb(size_t size, lv_color_format_t color_format)
{
    return lv_tlsf_malloc(image_tlsf, size + LV_DRAW_BUF_ALIGN - 1);
}

static void buf_free_cb(void * draw_buf)
{
    lv_tlsf_free(image_tlsf, draw_buf);
}

static void init_handlers(lv_draw_buf_handlers_t * handlers)
{
    handlers->buf_malloc_cb = buf_malloc_cb;
    handlers->buf_free_cb = buf_free_cb;
}

void lv_port_draw_buf_init(void)
{
    static uint32_t buf[2 * 1024 * 1024 / sizeof(uint32_t)];
    image_tlsf = lv_tlsf_create_with_pool(buf, sizeof(buf));

    init_handlers(lv_draw_buf_get_handlers());
    init_handlers(lv_draw_buf_get_font_handlers());
    init_handlers(lv_draw_buf_get_image_handlers());
}
