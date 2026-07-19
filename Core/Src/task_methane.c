/*
 * task_methane.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_methane.h"
#include "freertos_shared.h"
#include "sensor_adc.h"
#include "usart.h"
#include "cmsis_os.h"
#include <stdio.h>

void MethaneTask_Run(void *argument)
{
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(150);
  uint16_t methaneValue;
  BaseType_t bConversionOk;
#if DEBUG_UART_LOGGING
  char dbgBuf[64];
  int  dbgLen;
#endif
  /* Prime the pipeline: kick off the very first conversion before
     the periodic loop starts, so period 1's read has something
     to read. */
  ADC_HW_StartConversion(&hadc1);

  xLastWakeTime = xTaskGetTickCount();
  /* Infinite loop */
  for (;;)
  {
    vTaskDelayUntil(&xLastWakeTime, xPeriod);

    /* This conversion was started >=150 ms ago (max latency is
       50 ms) -> guaranteed complete. No polling needed for EOC;
       SR is only consulted for the error bit. */
    bConversionOk = ADC_HW_ReadValue(&hadc1, &methaneValue);

    /* Immediately displace: start the conversion this reading's
       "successor" will consume next period. */
    ADC_HW_StartConversion(&hadc1);

    if (bConversionOk)
    {
      osMutexAcquire(sensorDataMutexHandle, portMAX_DELAY);
      sharedSensorData.methaneLevel = methaneValue;
      sharedSensorData.methaneValid = pdTRUE;
      osMutexRelease(sensorDataMutexHandle);
#if DEBUG_UART_LOGGING
      osMutexAcquire(uartLogMutexHandle, osWaitForever);
      dbgLen = snprintf(dbgBuf, sizeof(dbgBuf), "CH4 raw=%u tick=%lu\r\n",
                         methaneValue, (unsigned long)xLastWakeTime);
      HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
      osMutexRelease(uartLogMutexHandle);
#endif
      if (methaneValue >= METHANE_CRITICAL_THRESHOLD)
      {
        // xSemaphoreGive(alarmEventSemaphore); turn alarm on
      }
    }
    else
    {
      /* device signaled an error in its status register —
         feed into your "one bad reading tolerated" logic here */
      osMutexAcquire(sensorDataMutexHandle, osWaitForever);
      sharedSensorData.methaneValid = pdFALSE;
      osMutexRelease(sensorDataMutexHandle);
    }
  }
}
