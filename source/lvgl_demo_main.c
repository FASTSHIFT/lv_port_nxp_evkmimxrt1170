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

static void AppTask(void *param)
{
    PRINTF("lvgl benchmark demo started\r\n");

    lv_port_pre_init();
    lv_init();
#if LV_USE_LOG
//    lv_log_register_print_cb(print_cb);
#endif

    lv_tick_set_cb(millis);
    lv_port_disp_init();
    lv_port_indev_init();
    lv_port_profiler_init();

    lv_demo_widgets();
    // lv_demo_benchmark();

    // lv_obj_t* obj = lv_obj_create(lv_scr_act());
    // lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    // lv_obj_center(obj);

    for (;;)
    {
        uint32_t idle = lv_task_handler();
        vTaskDelay(idle);
    }
}

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
