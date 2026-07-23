/*
 * task_methane.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_methane.h"
#include "task_alarm_manager.h"
#include "task_pump_manager.h"
#include "freertos_shared.h"
#include "sensor_adc.h"
#include "usart.h"
#include "cmsis_os.h"
#include <stdio.h>

void MethaneTask_Run(void *argument)
{
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(METHANE_TASK_PERIOD_MS);
  uint16_t methaneValue;
  BaseType_t bConversionOk;
  uint8_t consecutiveErrors = 0;
  /* Prime the pipeline: kick off the very first conversion before
     the periodic loop starts, so period 1's read has something
     to read. */
  ADC_HW_StartConversion(&hadc1);

  xLastWakeTime = xTaskGetTickCount();
  /* Infinite loop */
  for (;;)
  {
    vTaskDelayUntil(&xLastWakeTime, xPeriod);

    /* This conversion was started one period (100 ms) ago; ADC max
       latency is 50 ms -> guaranteed complete. No polling needed for
       EOC; SR is only consulted for the error bit. */
    bConversionOk = Sensor_ReadWithFaults(SIM_SENSOR_METHANE, &hadc1, &methaneValue);

    /* Immediately displace: start the conversion this reading's
       "successor" will consume next period. */
    ADC_HW_StartConversion(&hadc1);

    if (bConversionOk)
    {
      consecutiveErrors = 0;

      osMutexAcquire(sensorDataMutexHandle, portMAX_DELAY);
      sharedSensorData.methaneLevel = methaneValue;
      sharedSensorData.methaneValid = pdTRUE;
      osMutexRelease(sensorDataMutexHandle);

      if (methaneValue >= METHANE_CRITICAL_THRESHOLD)
      {
        PumpManager_SetMethaneCritical(pdTRUE);
        AlarmManager_RaiseCause(ALARM_BIT_METHANE);
      }
      else
      {
        PumpManager_SetMethaneCritical(pdFALSE);
      }
    }
    else
    {
      /* device signaled an error in its status register */
      osMutexAcquire(sensorDataMutexHandle, osWaitForever);
      sharedSensorData.methaneValid = pdFALSE;
      osMutexRelease(sensorDataMutexHandle);

      if (consecutiveErrors < 0xFF)  /* guard against wraparound */
      {
        consecutiveErrors++;
      }

      if (consecutiveErrors >= 2)
      {
        PumpManager_SetMethaneCritical(pdTRUE);
        AlarmManager_RaiseCause(ALARM_BIT_METHANE);
      }
    }
  }
}
