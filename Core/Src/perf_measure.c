/*
 * perf_measure.c
 *
 *  DWT-cycle-counter based per-task execution-time measurement.
 *  See perf_measure.h for the usage contract and the min-vs-max caveat.
 */

#include "perf_measure.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tim.h"        /* CubeMX-generated TIM2 handle (htim2) + MX_TIM2_Init */
#include <stdio.h>

/* One statistics slot per instrumented process. Each slot is written by a
 * single task only (indexed by its own PerfTaskId_t), so no lock is needed
 * on the write path. */
static PerfStats_t g_perf[PERF_TASK_COUNT];

/* Frequency of whatever time source Perf_CYCCNT() reads, in Hz. Set in
 * Perf_Init(): SystemCoreClock for DWT, or the TIM2 counter clock for TIM2.
 * cyc_to_us_x100() divides by this. */
static uint32_t g_perfTickHz = 0U;

/* Set by the Perf_Init() self-check: nonzero => the counter advances. */
static int g_timeBaseOk = 0;

int Perf_TimeBaseOk(void) { return g_timeBaseOk; }

static const char *const g_perfTaskNames[PERF_TASK_COUNT] =
{
    "Methane",
    "PumpFlow",
    "CO",
    "AirFlow",
    "WaterLevel",
    "PumpManager",
    "AlarmManager",
    "UIComms",
};

void Perf_Reset(void)
{
    for (int i = 0; i < PERF_TASK_COUNT; i++)
    {
        g_perf[i].count   = 0U;
        g_perf[i].minCyc  = UINT32_MAX;
        g_perf[i].maxCyc  = 0U;
        g_perf[i].lastCyc = 0U;
        g_perf[i].sumCyc  = 0U;
    }
}

/* Counter clock of the selected time source, in Hz. For TIM2 this is the
 * APB1 timer clock: equal to PCLK1 when the APB1 prescaler is /1, otherwise
 * 2 x PCLK1 (STM32 timer-clock doubling rule). */
static uint32_t Perf_TimeSourceHz(void)
{
#if PERF_TIME_SOURCE == PERF_SRC_TIM2
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1_Msk) >> RCC_CFGR_PPRE1_Pos;
    uint32_t timclk = (ppre1 < 4U) ? pclk1 : (pclk1 * 2U);  /* APB1 timer clock */
    /* Honour whatever prescaler CubeMX generated, so the .ioc stays the
     * single source of truth (PSC=0 -> full timer clock). */
    return timclk / (TIM2->PSC + 1U);
#else
    return SystemCoreClock;
#endif
}

void Perf_Init(void)
{
#if PERF_MEASURE_ENABLE
#if PERF_TIME_SOURCE == PERF_SRC_TIM2
    /* TIM2 is configured entirely by CubeMX (Internal Clock, PSC=0,
     * ARR=0xFFFFFFFF, up-counter) as the single source of truth; here we
     * only start the free-running counter. MX_TIM2_Init() must already
     * have run in main() before Perf_Init() (it does - peripheral init is
     * before USER CODE BEGIN 2). */
    HAL_TIM_Base_Start(&htim2);
#else
    /* Cortex-M DWT cycle counter (real silicon only; not modelled by Renode). */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
#endif

    g_perfTickHz = Perf_TimeSourceHz();

    /* Self-check: confirm the source actually advances, so a dead/unmodelled
     * counter is caught instead of silently reporting all-zero times. */
    uint32_t a = PERF_CYCCNT();
    for (volatile int i = 0; i < 200; i++) { __NOP(); }
    uint32_t b = PERF_CYCCNT();
    g_timeBaseOk = (b != a) ? 1 : 0;
#endif
    Perf_Reset();
}

void Perf_Record(PerfTaskId_t id, uint32_t cycles)
{
    if ((int)id >= PERF_TASK_COUNT)
    {
        return;
    }

    PerfStats_t *s = &g_perf[id];

    s->count++;
    s->lastCyc = cycles;
    s->sumCyc += cycles;

    if (cycles > s->maxCyc) { s->maxCyc = cycles; }
    if (cycles < s->minCyc) { s->minCyc = cycles; }
}

/* ticks -> microseconds * 100 (two decimal places), rounded.
 * us*100 = ticks * 100 * 1e6 / f = ticks * 1e8 / f, where f is the time
 * source clock (g_perfTickHz: CPU clock for DWT, timer clock for TIM2). */
static uint32_t cyc_to_us_x100(uint32_t cyc)
{
    if (g_perfTickHz == 0U)
    {
        return 0U;
    }
    uint64_t num = (uint64_t)cyc * 100000000ULL + (g_perfTickHz / 2U);
    return (uint32_t)(num / g_perfTickHz);
}

int Perf_FormatReport(char *buf, size_t len)
{
    /* Take a consistent snapshot so a mid-update slot can't produce a torn
     * average. Very short critical section (a struct copy). */
    PerfStats_t snap[PERF_TASK_COUNT];
    taskENTER_CRITICAL();
    for (int i = 0; i < PERF_TASK_COUNT; i++) { snap[i] = g_perf[i]; }
    taskEXIT_CRITICAL();

    int off = 0;
    off += snprintf(buf + off, (off < (int)len) ? (len - off) : 0,
                    "PERF (us, f=%luMHz, tb=%d)  n   min    avg    max\r\n",
                    (unsigned long)(g_perfTickHz / 1000000UL), g_timeBaseOk);

    for (int i = 0; i < PERF_TASK_COUNT; i++)
    {
        PerfStats_t *s = &snap[i];
        uint32_t minx = (s->count ? cyc_to_us_x100(s->minCyc) : 0U);
        uint32_t maxx = cyc_to_us_x100(s->maxCyc);
        uint32_t avgc = (s->count ? (uint32_t)(s->sumCyc / s->count) : 0U);
        uint32_t avgx = cyc_to_us_x100(avgc);

        off += snprintf(buf + off, (off < (int)len) ? (len - off) : 0,
                        "%-12s %5lu %4lu.%02lu %4lu.%02lu %4lu.%02lu\r\n",
                        g_perfTaskNames[i],
                        (unsigned long)s->count,
                        (unsigned long)(minx / 100U), (unsigned long)(minx % 100U),
                        (unsigned long)(avgx / 100U), (unsigned long)(avgx % 100U),
                        (unsigned long)(maxx / 100U), (unsigned long)(maxx % 100U));
    }
    return off;
}

int Perf_FormatJson(char *buf, size_t len)
{
    PerfStats_t snap[PERF_TASK_COUNT];
    taskENTER_CRITICAL();
    for (int i = 0; i < PERF_TASK_COUNT; i++) { snap[i] = g_perf[i]; }
    taskEXIT_CRITICAL();

    int off = 0;
    off += snprintf(buf + off, (off < (int)len) ? (len - off) : 0, "{\"perf\":{");

    for (int i = 0; i < PERF_TASK_COUNT; i++)
    {
        PerfStats_t *s = &snap[i];
        uint32_t minx = (s->count ? cyc_to_us_x100(s->minCyc) : 0U);
        uint32_t maxx = cyc_to_us_x100(s->maxCyc);
        uint32_t avgc = (s->count ? (uint32_t)(s->sumCyc / s->count) : 0U);
        uint32_t avgx = cyc_to_us_x100(avgc);

        /* Each task -> "Name":[count, min_x100, avg_x100, max_x100] */
        off += snprintf(buf + off, (off < (int)len) ? (len - off) : 0,
                        "%s\"%s\":[%lu,%lu,%lu,%lu]",
                        (i == 0) ? "" : ",",
                        g_perfTaskNames[i],
                        (unsigned long)s->count,
                        (unsigned long)minx,
                        (unsigned long)avgx,
                        (unsigned long)maxx);
    }

    /* Close the perf object and append a time-base health flag (tb=1 means
     * the counter is advancing). The UI ignores unknown keys. */
    off += snprintf(buf + off, (off < (int)len) ? (len - off) : 0,
                    "},\"tb\":%d}\r\n", g_timeBaseOk);
    return off;
}
