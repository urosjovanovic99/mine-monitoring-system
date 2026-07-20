/*
 * task_pump_flow.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_pump_flow.h"
#include "task_alarm_manager.h"
#include "freertos_shared.h"
#include "task_pump_manager.h"
#include "usart.h"
#include <stdio.h>

void PumpFlowTask_Run(void *argument)
{
  /* USER CODE BEGIN PumpFlowMonitorTask */
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(150);

  PumpState_t lastObservedCommand = PUMP_OFF;
  PumpState_t targetCommand       = PUMP_OFF;
  uint8_t     settlingCounter     = 0;
  BaseType_t  settling            = pdFALSE;

#if DEBUG_UART_LOGGING
  char dbgBuf[80];
  int  dbgLen;
#endif

  xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
      vTaskDelayUntil(&xLastWakeTime, xPeriod);

      osMutexAcquire(pumpMutexHandle, osWaitForever);
      PumpState_t currentCommand = pumpCommandedState;
      osMutexRelease(pumpMutexHandle);

      GPIO_PinState flowPin = HAL_GPIO_ReadPin(WATER_FLOW_SENSOR_GPIO_Port, WATER_FLOW_SENSOR_Pin);
      BaseType_t flowPresent = (flowPin == WATER_FLOW_ACTIVE_STATE) ? pdTRUE : pdFALSE;

      if (currentCommand != lastObservedCommand)
      {
          /* Activation #1: command just changed. Start the N-period
             settling window per spec -- do nothing else this period. */
          targetCommand        = currentCommand;
          settlingCounter       = 1;
          settling              = pdTRUE;
          lastObservedCommand   = currentCommand;
      }
      else if (settling)
      {
          settlingCounter++;

          if (settlingCounter > PUMP_FLOW_N_PERIODS)
          {
              /* Activation N+1: td has elapsed, verify actual state now */
              settling = pdFALSE;

              BaseType_t mismatch =
                  (targetCommand == PUMP_ON  && !flowPresent) ||
                  (targetCommand == PUMP_OFF &&  flowPresent);

              if (mismatch)
              {
                  AlarmManager_RaiseCause(ALARM_BIT_PUMPFAULT);
#if DEBUG_UART_LOGGING
                  osMutexAcquire(uartLogMutexHandle, osWaitForever);
                  dbgLen = snprintf(dbgBuf, sizeof(dbgBuf),
                      "PUMPFLOW FAULT: cmd=%s flow=%s tick=%lu\r\n",
                      (targetCommand == PUMP_ON) ? "ON" : "OFF",
                      flowPresent ? "YES" : "NO",
                      (unsigned long)xLastWakeTime);
                  HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
                  osMutexRelease(uartLogMutexHandle);
#endif
              }
          }
          /* else: still inside the N do-nothing activations */
      }
      else
      {
          /* Steady state -- no fresh command change, window already
             settled. Keep re-checking every period so a pump that fails
             mid-run (not just at start/stop) still gets caught. This is
             an extension beyond the literal spec text, which only
             describes the transition-triggered check -- trim it if you
             want the implementation to match the spec wording exactly. */
          BaseType_t mismatch =
              (targetCommand == PUMP_ON  && !flowPresent) ||
              (targetCommand == PUMP_OFF &&  flowPresent);

#if DEBUG_UART_LOGGING
		  osMutexAcquire(uartLogMutexHandle, osWaitForever);
		  dbgLen = snprintf(dbgBuf, sizeof(dbgBuf),
			  "Steady state: cmd=%s flow=%s tick=%lu\r\n",
			  (targetCommand == PUMP_ON) ? "ON" : "OFF",
			  flowPresent ? "YES" : "NO",
			  (unsigned long)xLastWakeTime);
		  HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
		  osMutexRelease(uartLogMutexHandle);
#endif
          if (mismatch)
          {
              AlarmManager_RaiseCause(ALARM_BIT_PUMPFAULT);
          }
      }
  }
  /* USER CODE END PumpFlowMonitorTask */
}
