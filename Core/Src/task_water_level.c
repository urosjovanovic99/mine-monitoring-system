/*
 * task_water_level.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_water_level.h"
#include "freertos_shared.h"
#include "cmsis_os.h"
#include "usart.h"
#include <string.h>

void WaterLevelTask_Run(void *argument)
{
  /* Infinite loop */
  for (;;)
  {
    if (osSemaphoreAcquire(waterLevelSemaphoreHandle, osWaitForever) == osOK)
    {
      GPIO_PinState highSet = HAL_GPIO_ReadPin(HIGH_WATER_GPIO_Port, HIGH_WATER_Pin);
      GPIO_PinState lowSet  = HAL_GPIO_ReadPin(LOW_WATER_GPIO_Port, LOW_WATER_Pin);

      WaterLevelEvent_t evt;
      const char *msg;
      if (highSet == GPIO_PIN_SET)
      {
        evt = WATER_LEVEL_HIGH;
        msg = "WaterLevelTask: HIGH -> pump ON\r\n";
      }
      else if (lowSet == GPIO_PIN_SET)
      {
        evt = WATER_LEVEL_LOW;
        msg = "WaterLevelTask: LOW -> pump OFF\r\n";
      }
      else
      {
        evt = WATER_LEVEL_NORMAL;
        msg = "WaterLevelTask: NORMAL (no action)\r\n";
      }

      /* TEMPORARY test-only output so the EXTI->task path is observable in
       * Renode. Remove or move into UICommsTask once real JSON telemetry is back. */
      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

      osMessageQueuePut(pumpCommandQueueHandle, &evt, 0, 0);
    }
  }
}
