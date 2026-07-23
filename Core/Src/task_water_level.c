/*
 * task_water_level.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_water_level.h"
#include "freertos_shared.h"
#include "cmsis_os.h"

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
      /* Read the current level FIRST, then classify. */
      GPIO_PinState highSet = HAL_GPIO_ReadPin(HIGH_WATER_GPIO_Port, HIGH_WATER_Pin);
      GPIO_PinState lowSet  = HAL_GPIO_ReadPin(LOW_WATER_GPIO_Port, LOW_WATER_Pin);

      WaterLevelEvent_t evt;
      if (highSet == GPIO_PIN_SET)
      {
        evt = WATER_LEVEL_HIGH;
      }
      else if (lowSet == GPIO_PIN_SET)
      {
        evt = WATER_LEVEL_LOW;
      }
      else
      {
        evt = WATER_LEVEL_NORMAL;
      }

      uint32_t now = osKernelGetTickCount();
      if (hasAccepted && evt == lastEvent && (now - lastEventTick) < WATER_LEVEL_DEBOUNCE_MS)
      {
        continue; /* same state within debounce window - ignore bounce */
      }
      lastEventTick = now;
      lastEvent     = evt;
      hasAccepted   = 1U;

      osMessageQueuePut(pumpCommandQueueHandle, &evt, 0, 0);
    }
  }
}
