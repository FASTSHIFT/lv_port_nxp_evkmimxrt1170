/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "task.h"

#include "fsl_debug_console.h"
#include "lvgl_support.h"
#include "pin_mux.h"
#include "board.h"
#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"

#include "fsl_soc_src.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
#if LV_USE_LOG
static void print_cb(lv_log_level_t level, const char *buf)
{
    PRINTF("%s", buf);
}
#endif

static uint32_t millis(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void profiler_timer_cb(lv_timer_t *timer)
{
#if LV_USE_PROFILER
    lv_profiler_builtin_set_enable(true);
#endif
}

static void AppTask(void *param)
{
    PRINTF("lvgl benchmark demo started\r\n");

    lv_port_pre_init();
    lv_init();
#if LV_USE_LOG
//    lv_log_register_print_cb(print_cb);
#endif

    lv_port_draw_buf_init();
    lv_tick_set_cb(millis);
    lv_port_disp_init();
    lv_port_indev_init();
    lv_port_profiler_init();

//    lv_demo_widgets();
    lv_demo_benchmark();

    lv_timer_t *timer = lv_timer_create(profiler_timer_cb, 5000, NULL);
    lv_timer_set_repeat_count(timer, 1);

    for (;;)
    {
        uint32_t idle = lv_task_handler();
        vTaskDelay(idle);
    }
}

#if LV_USE_OS == LV_OS_NONE

static uint32_t freertos_task_switch_timestamp = 0;
static uint32_t freertos_idle_task_running = 0;
static uint32_t freertos_non_idle_time_sum = 0;
static uint32_t freertos_idle_time_sum = 0;

void lv_freertos_task_switch_in(const char * name)
{
    if(lv_strcmp(name, "IDLE")) freertos_idle_task_running = false;
    else freertos_idle_task_running = true;

    freertos_task_switch_timestamp = lv_tick_get();
}

void lv_freertos_task_switch_out(void)
{
    uint32_t elaps = lv_tick_elaps(freertos_task_switch_timestamp);
    if(freertos_idle_task_running) freertos_idle_time_sum += elaps;
    else freertos_non_idle_time_sum += elaps;
}

uint32_t freertos_get_idle_percent(void)
{
    if(freertos_non_idle_time_sum + freertos_idle_time_sum == 0) {
        LV_LOG_WARN("Not enough time elapsed to provide idle percentage");
        return 0;
    }

    uint32_t pct = (freertos_idle_time_sum * 100) / (freertos_idle_time_sum +
                                                              freertos_non_idle_time_sum);

    freertos_non_idle_time_sum = 0;
    freertos_idle_time_sum = 0;

    return pct;
}

#else

uint32_t freertos_get_idle_percent(void)
{
    return lv_os_get_idle_percent();
}

#endif

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Main function
 */
int main(void)
{
    BaseType_t stat;

    /* Init board hardware. */

    BOARD_ConfigMPU();
    BOARD_BootClockRUN();

    /*
     * Reset the displaymix, otherwise during debugging, the
     * debugger may not reset the display, then the behavior
     * is not right.
     */
    SRC_AssertSliceSoftwareReset(SRC, kSRC_DisplaySlice);

    BOARD_InitLpuartPins();
#ifndef DISABLE_DISPLAY
    BOARD_InitMipiPanelPins();
#endif
#ifndef DISABLE_TOUCH
    BOARD_MIPIPanelTouch_I2C_Init();
#endif
    BOARD_InitDebugConsole();

    stat = xTaskCreate(AppTask, "lvgl", configMINIMAL_STACK_SIZE + 4096, NULL, tskIDLE_PRIORITY + 2, NULL);

    if (pdPASS != stat)
    {
        PRINTF("Failed to create lvgl task");
        while (1)
            ;
    }

    vTaskStartScheduler();

    for (;;)
    {
    } /* should never get here */
}

/*!
 * @brief Malloc failed hook.
 */
void vApplicationMallocFailedHook(void)
{
    PRINTF("Malloc failed. Increase the heap size.");

    for (;;)
        ;
}

/*!
 * @brief FreeRTOS tick hook.
 */
void vApplicationTickHook(void)
{
}

/*!
 * @brief Stack overflow hook.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)pcTaskName;
    (void)xTask;

    for (;;)
        ;
}
