#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*
 * FreeRTOS configuration for STM32F446RE @ 180 MHz
 * Port: GCC/ARM_CM4F
 *
 * configSYSTICK_CLOCK_HZ must equal HCLK after SystemClock_Config().
 * The ARM_CM4F port uses this value to program the SysTick reload register.
 */

/* ── Clocks ──────────────────────────────────────────────────────────────── */
#define configCPU_CLOCK_HZ              ( 180000000UL )
/* NOTE: do NOT define configSYSTICK_CLOCK_HZ.
 * When defined, the ARM_CM4F port.c sets portNVIC_SYSTICK_CLK_BIT_CONFIG=0
 * which selects the external reference clock (HCLK/8 = 22.5 MHz), while the
 * reload value is still calculated from configCPU_CLOCK_HZ (180 MHz).
 * That mismatch makes ticks 8x too slow (8 ms instead of 1 ms).
 * Leaving it undefined makes the port default to configCPU_CLOCK_HZ and use
 * CLKSOURCE=1 (processor clock = HCLK), giving correct 1 ms ticks. */

/* ── Scheduler ───────────────────────────────────────────────────────────── */
#define configUSE_PREEMPTION            1
#define configUSE_TIME_SLICING          1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1

/* ── Tick rate ────────────────────────────────────────────────────────────── */
#define configTICK_RATE_HZ              ( ( TickType_t ) 1000 )

/* ── Priorities ──────────────────────────────────────────────────────────── */
#define configMAX_PRIORITIES            ( 5 )

/* ── Stack sizes (in words) ──────────────────────────────────────────────── */
#define configMINIMAL_STACK_SIZE        ( ( uint16_t ) 128 )
#define configNORMAL_STACK_SIZE         ( ( uint16_t ) 256 )

/* ── Heap (managed by heap_4.c) ──────────────────────────────────────────── */
#define configTOTAL_HEAP_SIZE           ( ( size_t ) ( 10 * 1024 ) )

/* ── Misc ────────────────────────────────────────────────────────────────── */
#define configMAX_TASK_NAME_LEN         ( 16 )
#define configTICK_TYPE_WIDTH_IN_BITS   TICK_TYPE_WIDTH_32_BITS

/* ── Hooks ───────────────────────────────────────────────────────────────── */
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configUSE_PASSIVE_IDLE_HOOK     0
#define configUSE_MALLOC_FAILED_HOOK    1

/* ── Memory allocation ───────────────────────────────────────────────────── */
#define configSUPPORT_STATIC_ALLOCATION  0
#define configSUPPORT_DYNAMIC_ALLOCATION 1

/* ── Optional features ───────────────────────────────────────────────────── */
#define configUSE_MUTEXES               1
#define configUSE_RECURSIVE_MUTEXES     0
#define configUSE_COUNTING_SEMAPHORES   0
#define configUSE_QUEUE_SETS            0
#define configQUEUE_REGISTRY_SIZE       0
#define configUSE_TIMERS                0
#define configUSE_EVENT_GROUPS          1

/* ── Stack overflow detection (method 2: write + check a canary pattern) ─── */
#define configCHECK_FOR_STACK_OVERFLOW  2

/* ── Run-time stats (disabled) ───────────────────────────────────────────── */
#define configGENERATE_RUN_TIME_STATS   0
#define configUSE_TRACE_FACILITY        0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0

/* ── Co-routines (legacy, unused) ────────────────────────────────────────── */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES ( 1 )

/* ── Cortex-M interrupt priority configuration ───────────────────────────────
 * STM32F4 implements 4 priority bits (16 levels, 0 = highest).
 * ALL priority bits must be preemption bits → NVIC_PRIORITYGROUP_4.
 *
 * configKERNEL_INTERRUPT_PRIORITY:
 *   SysTick and PendSV run at the lowest possible priority (15 → 0xFF).
 *
 * configMAX_SYSCALL_INTERRUPT_PRIORITY:
 *   Highest priority that may call FreeRTOS fromISR API (level 5 → 0x50).
 *   Any ISR with a numerically lower value (higher urgency) must NOT call
 *   FreeRTOS API functions.
 *
 * The shift by (8 - 4) = 4 places the 4-bit value in the upper nibble of
 * the 8-bit NVIC priority register.
 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY   5

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - 4 ) )

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - 4 ) )

/* ── Assert ──────────────────────────────────────────────────────────────── */
#define configASSERT( x ) \
    if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for(;;); }

/* ── Map FreeRTOS port handler names to CMSIS IRQ handler names ─────────────
 * The ARM_CM4F port defines:
 *   vPortSVCHandler, xPortPendSVHandler, xPortSysTickHandler
 * The startup vector table expects:
 *   SVC_Handler, PendSV_Handler, SysTick_Handler
 *
 * These macros rename the port functions at compile time so they satisfy the
 * vector table symbols without any call-wrapper overhead. Do NOT define
 * SVC_Handler / PendSV_Handler / SysTick_Handler in stm32f4xx_it.c.
 */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

/* ── API includes ────────────────────────────────────────────────────────── */
#define INCLUDE_vTaskDelay                    1
#define INCLUDE_xTaskDelayUntil               1
#define INCLUDE_vTaskDelete                   1
#define INCLUDE_vTaskSuspend                  1
#define INCLUDE_xTaskGetSchedulerState        1
#define INCLUDE_uxTaskGetStackHighWaterMark   1

#endif /* FREERTOS_CONFIG_H */
