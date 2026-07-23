/*
 * task_airflow.c
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#include "task_airflow.h"
#include "task_alarm_manager.h"
#include "freertos_shared.h"
#include "sensor_adc.h"
#include "usart.h"
#include "cmsis_os.h"
#include <stdio.h>

void AirFlowTask_Run(void *argument)
{
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(AIRFLOW_TASK_PERIOD_MS);
  uint16_t airFlowValue;
  BaseType_t bConversionOk;
  uint8_t consecutiveErrors = 0;

  ADC_HW_StartConversion(&hadc3);

  xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&xLastWakeTime, xPeriod);

    bConversionOk = ADC_HW_ReadValue(&hadc3, &airFlowValue);
    ADC_HW_StartConversion(&hadc3);

    if (bConversionOk)
    {
      consecutiveErrors = 0;

      osMutexAcquire(sensorDataMutexHandle, osWaitForever);
      sharedSensorData.airFlowLevel = airFlowValue;
      sharedSensorData.airFlowValid = pdTRUE;
      osMutexRelease(sensorDataMutexHandle);

      /* Air flow alarms on too LOW a reading, not too high -
         insufficient ventilation is the fault condition here. */
      if (airFlowValue <= AIRFLOW_LOW_THRESHOLD)
      {
        AlarmManager_RaiseCause(ALARM_BIT_AIRFLOW);
      }
    }
    else
    {   // error recovery still have to analyze this part of code
      osMutexAcquire(sensorDataMutexHandle, osWaitForever);
      sharedSensorData.airFlowValid = pdFALSE;
      osMutexRelease(sensorDataMutexHandle);

      if (consecutiveErrors < 0xFF)  /* guard against wraparound */
      {
        consecutiveErrors++;
      }

      /* Spec requirement: 2 consecutive read errors -> alarm */
      if (consecutiveErrors >= 2)
      {
        AlarmManager_RaiseCause(ALARM_BIT_AIRFLOW);
      }
    }
  }
}
