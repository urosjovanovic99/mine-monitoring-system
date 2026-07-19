/*
 * task_co_sensor.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_co_sensor.h"
#include "freertos_shared.h"
#include "sensor_adc.h"
#include "usart.h"
#include "cmsis_os.h"
#include <stdio.h>

void COSensorTask_Run(void *argument)
{
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(150);
  uint16_t coValue;
  BaseType_t bConversionOk;

#if DEBUG_UART_LOGGING
  char dbgBuf[64];
  int  dbgLen;
#endif

  ADC_HW_StartConversion(&hadc2);

  xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&xLastWakeTime, xPeriod);

    bConversionOk = ADC_HW_ReadValue(&hadc2, &coValue);
    ADC_HW_StartConversion(&hadc2);

    if (bConversionOk)
    {
      osMutexAcquire(sensorDataMutexHandle, osWaitForever);
      sharedSensorData.coLevel = coValue;
      sharedSensorData.coValid = pdTRUE;
      osMutexRelease(sensorDataMutexHandle);

#if DEBUG_UART_LOGGING
      osMutexAcquire(uartLogMutexHandle, osWaitForever);
      dbgLen = snprintf(dbgBuf, sizeof(dbgBuf), "CO raw=%u tick=%lu\r\n",
                         coValue, (unsigned long)xLastWakeTime);
      HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
      osMutexRelease(uartLogMutexHandle);
#endif

      if (coValue >= CO_CRITICAL_THRESHOLD)
      {
        // osSemaphoreRelease(alarmEventSemaphoreHandle); turn alarm on
      }
    }
    else
    {
      osMutexAcquire(sensorDataMutexHandle, osWaitForever);
      sharedSensorData.coValid = pdFALSE;
      osMutexRelease(sensorDataMutexHandle);
    }
  }
}
