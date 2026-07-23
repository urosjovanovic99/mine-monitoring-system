/*
 * sim_env.c
 *
 * Implementation of the interactive simulation / test-harness layer.
 * Entirely compiled out unless SIMULATION_BUILD is defined in sim_env.h.
 *
 *  Created on: Jul 24, 2026
 *      Author: uros.jovanovic
 */

#include "sim_env.h"

#ifdef SIMULATION_BUILD

#include "freertos_shared.h"   /* simMutexHandle, pumpMutexHandle, waterLevelSemaphoreHandle */
#include "cmsis_os.h"
#include "main.h"              /* HAL_GetTick() */

/* ---- simulated environment state (all guarded by simMutexHandle) ------- */
static uint8_t  s_failPercent[SIM_SENSOR_COUNT] = { 0, 0, 0 };
static uint16_t s_forceFail[SIM_SENSOR_COUNT]   = { 0, 0, 0 };

static float    s_tankLevel = 50.0f;   /* start mid-tank: NORMAL */
static float    s_fillRate  = 1.0f;    /* %/s while pump OFF (water seeping in) */
static float    s_drainRate = 3.0f;    /* %/s while pump ON  (water pumped out) */

/* xorshift32 PRNG - a real rand() pulls in newlib and is not reentrant-safe
 * across tasks; this is tiny, deterministic and self-contained. */
static uint32_t s_rng = 0x1234ABCDu;

static uint32_t Sim_Rand(void)
{
    uint32_t x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x;
    return x;
}

void Sim_Init(void)
{
    /* Seed from the boot tick so repeated runs are not identical. */
    s_rng ^= (HAL_GetTick() | 1u);
}

/* ------------------------------------------------------------------------ *
 * Sensor read-failure injection
 * ------------------------------------------------------------------------ */
BaseType_t Sim_ShouldFailRead(SimSensor_t sensor)
{
    if (sensor >= SIM_SENSOR_COUNT)
    {
        return pdFALSE;
    }

    BaseType_t fail = pdFALSE;

    osMutexAcquire(simMutexHandle, osWaitForever);

    if (s_forceFail[sensor] > 0)
    {
        s_forceFail[sensor]--;      /* deterministic forced failure */
        fail = pdTRUE;
    }
    else if (s_failPercent[sensor] > 0)
    {
        if ((Sim_Rand() % 100u) < (uint32_t)s_failPercent[sensor])
        {
            fail = pdTRUE;
        }
    }

    osMutexRelease(simMutexHandle);
    return fail;
}

void Sim_SetFailPercent(SimSensor_t sensor, uint8_t percent)
{
    if (sensor >= SIM_SENSOR_COUNT)
    {
        return;
    }
    if (percent > 100u)
    {
        percent = 100u;
    }
    osMutexAcquire(simMutexHandle, osWaitForever);
    s_failPercent[sensor] = percent;
    osMutexRelease(simMutexHandle);
}

void Sim_ForceFail(SimSensor_t sensor, uint16_t count)
{
    if (sensor >= SIM_SENSOR_COUNT)
    {
        return;
    }
    osMutexAcquire(simMutexHandle, osWaitForever);
    s_forceFail[sensor] = count;
    osMutexRelease(simMutexHandle);
}

/* ------------------------------------------------------------------------ *
 * Water-tank plant model
 * ------------------------------------------------------------------------ */
void Sim_SetWaterRates(float fillPctPerSec, float drainPctPerSec)
{
    if (fillPctPerSec  < 0.0f) fillPctPerSec  = 0.0f;
    if (drainPctPerSec < 0.0f) drainPctPerSec = 0.0f;
    osMutexAcquire(simMutexHandle, osWaitForever);
    s_fillRate  = fillPctPerSec;
    s_drainRate = drainPctPerSec;
    osMutexRelease(simMutexHandle);
}

void Sim_TankStep(uint32_t dtMs, PumpState_t pump)
{
    float dt = (float)dtMs / 1000.0f;

    osMutexAcquire(simMutexHandle, osWaitForever);
    /* Pump running removes water; otherwise the sump keeps filling. */
    if (pump == PUMP_ON)
    {
        s_tankLevel -= s_drainRate * dt;
    }
    else
    {
        s_tankLevel += s_fillRate * dt;
    }
    if (s_tankLevel > 100.0f) s_tankLevel = 100.0f;
    if (s_tankLevel < 0.0f)   s_tankLevel = 0.0f;
    osMutexRelease(simMutexHandle);
}

WaterLevelEvent_t Sim_TankClassify(void)
{
    WaterLevelEvent_t evt;

    osMutexAcquire(simMutexHandle, osWaitForever);
    if (s_tankLevel >= SIM_TANK_HIGH_PCT)
    {
        evt = WATER_LEVEL_HIGH;
    }
    else if (s_tankLevel <= SIM_TANK_LOW_PCT)
    {
        evt = WATER_LEVEL_LOW;
    }
    else
    {
        evt = WATER_LEVEL_NORMAL;
    }
    osMutexRelease(simMutexHandle);
    return evt;
}

uint8_t Sim_TankLevelPct(void)
{
    uint8_t pct;
    osMutexAcquire(simMutexHandle, osWaitForever);
    pct = (uint8_t)(s_tankLevel + 0.5f);
    osMutexRelease(simMutexHandle);
    return pct;
}

/* ------------------------------------------------------------------------ *
 * Plant task
 *
 * Integrates the tank level every SIM_TANK_STEP_MS and, on a threshold
 * crossing, releases waterLevelSemaphoreHandle - the exact same trigger the
 * physical HIGH/LOW EXTI ISR would raise. WaterLevelTask then runs unchanged:
 * it re-reads the (now simulated) level via Sim_TankClassify(), debounces,
 * publishes waterLevelState and posts the event onto pumpCommandQueue.
 *
 * NOTE for Zadatak 3: this is test scaffolding, not a system process. When
 * SIMULATION_BUILD is disabled it does not exist. Exclude it from the
 * response-time analysis of the real controller (or model it separately).
 * ------------------------------------------------------------------------ */
void SimTankTask_Run(void *argument)
{
    (void)argument;

    TickType_t        xLastWakeTime = xTaskGetTickCount();
    const TickType_t  xPeriod       = pdMS_TO_TICKS(SIM_TANK_STEP_MS);
    WaterLevelEvent_t lastEvt       = Sim_TankClassify();

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        osMutexAcquire(pumpMutexHandle, osWaitForever);
        PumpState_t pump = pumpCommandedState;
        osMutexRelease(pumpMutexHandle);

        Sim_TankStep(SIM_TANK_STEP_MS, pump);

        WaterLevelEvent_t evt = Sim_TankClassify();
        if (evt != lastEvt)
        {
            lastEvt = evt;
            osSemaphoreRelease(waterLevelSemaphoreHandle);
        }
    }
}

#endif /* SIMULATION_BUILD */
