/*
 * task_water_level.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_water_level.h"
#include "freertos_shared.h"
#include "cmsis_os.h"
#include "perf_measure.h"

volatile WaterLevelEvent_t waterLevelState = WATER_LEVEL_NORMAL;
volatile int32_t waterSimRate_mm_s = 0;
volatile int32_t waterSimLevel_mm  = WATER_SIM_START_MM;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if ((GPIO_Pin == HIGH_WATER_Pin || GPIO_Pin == LOW_WATER_Pin) && !simModeEnabled)
  {
    osSemaphoreRelease(waterLevelSemaphoreHandle);
  }
}

static WaterLevelEvent_t WaterLevel_Sample(void)
{
  if (simModeEnabled)
  {
    if (waterSimLevel_mm >= WATER_SIM_HIGH_MM) return WATER_LEVEL_HIGH;
    if (waterSimLevel_mm <= WATER_SIM_LOW_MM)  return WATER_LEVEL_LOW;
    return WATER_LEVEL_NORMAL;
  }

  if (HAL_GPIO_ReadPin(HIGH_WATER_GPIO_Port, HIGH_WATER_Pin) == GPIO_PIN_SET)
  {
    return WATER_LEVEL_HIGH;
  }
  if (HAL_GPIO_ReadPin(LOW_WATER_GPIO_Port, LOW_WATER_Pin) == GPIO_PIN_SET)
  {
    return WATER_LEVEL_LOW;
  }
  return WATER_LEVEL_NORMAL;
}

void WaterSim_TimerCb(void *argument)
{
  (void)argument;
  static WaterLevelEvent_t lastLogical = WATER_LEVEL_NORMAL;

  if (!simModeEnabled)
  {
    return;
  }

  int32_t lvl = waterSimLevel_mm
              + (waterSimRate_mm_s * (int32_t)WATER_SIM_TICK_MS) / 1000;
  if (lvl < 0)               { lvl = 0; }
  if (lvl > WATER_SIM_MAX_MM) { lvl = WATER_SIM_MAX_MM; }
  waterSimLevel_mm = lvl;

  WaterLevelEvent_t now = WaterLevel_Sample();
  if (now != lastLogical)
  {
    lastLogical = now;
    osSemaphoreRelease(waterLevelSemaphoreHandle);
  }
}

void WaterLevelTask_Run(void *argument)
{
  uint32_t          lastEventTick = 0U;
  WaterLevelEvent_t lastEvent     = WATER_LEVEL_NORMAL;
  uint8_t           hasAccepted   = 0U;

  /* Infinite loop */
  for (;;)
  {
    if (osSemaphoreAcquire(waterLevelSemaphoreHandle, osWaitForever) == osOK)
    {
      MEASURE_EXECUTION_TIME_BEGIN();
      /* Sample the current level (real pins, or the simulated stimulus). */
      WaterLevelEvent_t evt = WaterLevel_Sample();

      uint32_t now = osKernelGetTickCount();
      if (hasAccepted && evt == lastEvent && (now - lastEventTick) < WATER_LEVEL_DEBOUNCE_MS)
      {
        continue; /* same state within debounce window - ignore bounce */
      }
      lastEventTick = now;
      lastEvent     = evt;
      hasAccepted   = 1U;

      waterLevelState = evt; /* publish latest level for UI telemetry */

      osMessageQueuePut(pumpCommandQueueHandle, &evt, 0, 0);

      MEASURE_EXECUTION_TIME_END(PERF_TASK_WATERLEVEL);
    }
  }
}
