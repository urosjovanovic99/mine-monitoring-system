/*
 * task_co_sensor.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_co_sensor.h"
#include "task_alarm_manager.h"
#include "freertos_shared.h"
#include "sensor_adc.h"
#include "usart.h"
#include "cmsis_os.h"
#include <stdio.h>

void COSensorTask_Run(void *argument)
{
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(CO_TASK_PERIOD_MS);
  uint16_t coValue;
  BaseType_t bConversionOk;
  uint8_t consecutiveErrors = 0;

  ADC_HW_StartConversion(&hadc2);

  xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&xLastWakeTime, xPeriod);

    bConversionOk = Sensor_ReadWithFaults(SIM_SENSOR_CO, &hadc2, &coValue);
    ADC_HW_StartConversion(&hadc2);

    if (bConversionOk)
    {
      consecutiveErrors = 0;

      osMutexAcquire(sensorDataMutexHandle, osWaitForever);
      sharedSensorData.coLevel = coValue;
      sharedSensorData.coValid = pdTRUE;
      osMutexRelease(sensorDataMutexHandle);

      if (coValue >= CO_CRITICAL_THRESHOLD)
      {
        AlarmManager_RaiseCause(ALARM_BIT_CO);
      }
    }
    else
    {
      osMutexAcquire(sensorDataMutexHandle, osWaitForever);
      sharedSensorData.coValid = pdFALSE;
      osMutexRelease(sensorDataMutexHandle);

      if (consecutiveErrors < 0xFF)  /* guard against wraparound */
      {
        consecutiveErrors++;
      }

      /* Spec requirement: 2 consecutive read errors -> alarm */
      if (consecutiveErrors >= 2)
      {
        AlarmManager_RaiseCause(ALARM_BIT_CO);
      }
    }
  }
}
