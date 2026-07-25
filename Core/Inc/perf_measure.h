/*
 * perf_measure.h
 *
 *  Per-task worst-case execution-time (WCET) instrumentation based on the
 *  Cortex-M4 DWT cycle counter (DWT->CYCCNT). At 180 MHz one cycle is
 *  ~5.56 ns, so the counter gives sub-microsecond resolution.
 *
 *  Usage inside a task body (measure ONE activation, AFTER the blocking
 *  wait such as vTaskDelayUntil / osSemaphoreAcquire / osMessageQueueGet
 *  has returned):
 *
 *      for (;;)
 *      {
 *          vTaskDelayUntil(&t, period);       // blocking wait - NOT measured
 *          MEASURE_EXECUTION_TIME_BEGIN();    // start stopwatch
 *              ... per-activation work ...
 *          MEASURE_EXECUTION_TIME_END(PERF_TASK_METHANE);
 *      }
 *
 *  or, for a self-contained block:
 *
 *      MEASURE_EXECUTION_TIME(PERF_TASK_METHANE, {
 *          ... per-activation work ...
 *      });
 *
 *  IMPORTANT - what this measures:
 *    The BEGIN->END delta is ELAPSED wall-clock time. For the highest-
 *    priority task it equals pure execution time C. For a lower-priority
 *    task it also includes any preemption by higher-priority tasks/ISRs
 *    that happened during the body. To recover the pure (un-preempted) C
 *    that the response-time test needs: drive the WORST-CASE inputs
 *    continuously (constant read errors, critical gas levels, pump
 *    transitions, full telemetry) so every activation takes the worst
 *    code path, then read the reported MIN - that sample is the one that
 *    ran without being preempted. The analytic RTA adds interference on
 *    top of C, so feeding an elapsed MAX back as C would double-count it.
 */

#ifndef INC_PERF_MEASURE_H_
#define INC_PERF_MEASURE_H_

#include <stdint.h>
#include <stddef.h>
#include "main.h"   /* pulls in CMSIS core -> CoreDebug / DWT definitions */

#ifdef __cplusplus
extern "C" {
#endif

/* Master switch. Set to 0 for a production build: all macros below become
 * no-ops and add zero code / zero cycles. */
#ifndef PERF_MEASURE_ENABLE
#define PERF_MEASURE_ENABLE  1
#endif

/* One slot per instrumented process. Keep in sync with g_perfTaskNames[]. */
typedef enum
{
    PERF_TASK_METHANE = 0,
    PERF_TASK_PUMPFLOW,
    PERF_TASK_CO,
    PERF_TASK_AIRFLOW,
    PERF_TASK_WATERLEVEL,
    PERF_TASK_PUMPMANAGER,
    PERF_TASK_ALARMMANAGER,
    PERF_TASK_UICOMMS,
    PERF_TASK_COUNT
} PerfTaskId_t;

typedef struct
{
    uint32_t count;    /* number of recorded activations           */
    uint32_t minCyc;   /* smallest delta seen (best C estimate)    */
    uint32_t maxCyc;   /* largest delta seen (elapsed, w/ preempt) */
    uint32_t lastCyc;  /* most recent delta                        */
    uint64_t sumCyc;   /* running sum -> average                   */
} PerfStats_t;

/* Enable DWT->CYCCNT and clear all statistics. Call once from main(),
 * before osKernelStart(). */
void Perf_Init(void);

/* Fold one measured interval (in CPU cycles) into task 'id' statistics.
 * Single-writer per slot (each task uses its own id) -> lock-free. */
void Perf_Record(PerfTaskId_t id, uint32_t cycles);

/* Reset all counters (e.g. after warm-up, to start a clean measurement
 * window under worst-case stimulus). */
void Perf_Reset(void);

/* Format a human-readable report (count / min / avg / max in microseconds)
 * into 'buf'. Returns the number of bytes written (excluding the NUL). */
int  Perf_FormatReport(char *buf, size_t len);

/* Format a machine-readable JSON line for the telemetry UI:
 *   {"perf":{"Methane":[n,min,avg,max], ...}}\r\n
 * The min/avg/max fields are in MICROSECONDS x100 (fixed-point, two
 * decimals) so no float printf is needed on the MCU - the host divides
 * by 100.0. Returns bytes written (excluding NUL). */
int  Perf_FormatJson(char *buf, size_t len);

/* How many UICommsTask periods elapse between automatic perf dumps when
 * PERF_MEASURE_ENABLE is set. 7 * 150 ms ~= 1 s. */
#ifndef PERF_STREAM_DIVIDER
#define PERF_STREAM_DIVIDER  7u
#endif

/* --- Time source selection -------------------------------------------------
 * Renode does NOT model the Cortex-M DWT cycle counter (DWT->CYCCNT stays 0),
 * which is why measurements read 0 on the emulator. So we default to a
 * free-running 32-bit hardware timer (TIM2) that Renode DOES model. On real
 * silicon set PERF_TIME_SOURCE = PERF_SRC_DWT for true cycle resolution.
 * Either way the "tick" is 32-bit and the unsigned (end - start) subtraction
 * stays correct across one wrap, which every task body is far shorter than. */
#define PERF_SRC_DWT    0
#define PERF_SRC_TIM2   1
#ifndef PERF_TIME_SOURCE
#define PERF_TIME_SOURCE  PERF_SRC_TIM2
#endif

/* Returns nonzero if Perf_Init() confirmed the chosen time source actually
 * advances (guards against a dead/unmodelled counter reporting all zeros). */
int Perf_TimeBaseOk(void);

#if PERF_MEASURE_ENABLE

#if PERF_TIME_SOURCE == PERF_SRC_TIM2
#define PERF_CYCCNT()   (TIM2->CNT)
#else
#define PERF_CYCCNT()   (DWT->CYCCNT)
#endif

#define MEASURE_EXECUTION_TIME_BEGIN() \
    uint32_t _perf_start = PERF_CYCCNT()

#define MEASURE_EXECUTION_TIME_END(id) \
    Perf_Record((id), (uint32_t)(PERF_CYCCNT() - _perf_start))

/* Convenience wrapper for a self-contained statement block. Do NOT use this
 * form if 'code_block' contains continue/break/return meant for the
 * enclosing loop - use the BEGIN/END pair there instead. */
#define MEASURE_EXECUTION_TIME(id, code_block)                 \
    do {                                                       \
        uint32_t _perf_start = PERF_CYCCNT();                  \
        { code_block }                                         \
        Perf_Record((id), (uint32_t)(PERF_CYCCNT() - _perf_start)); \
    } while (0)

#else /* measurement disabled - zero-overhead no-ops */

#define PERF_CYCCNT()                        (0U)
#define MEASURE_EXECUTION_TIME_BEGIN()       ((void)0)
#define MEASURE_EXECUTION_TIME_END(id)       ((void)0)
#define MEASURE_EXECUTION_TIME(id, code_block) do { code_block } while (0)

#endif /* PERF_MEASURE_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* INC_PERF_MEASURE_H_ */
