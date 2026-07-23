/*
 * task_pump_manager.c
 *
 *  Created on: Jul 20, 2026
 *      Author: uros.jovanovic
 */

#include "task_pump_manager.h"
#include "task_alarm_manager.h"
#include "freertos_shared.h"
#include "task_methane.h"
#include "task_co_sensor.h"
#include "task_airflow.h"
#include "main.h"
#include "usart.h"
#include "cmsis_os.h"
#include <stdio.h>

PumpState_t pumpCommandedState = PUMP_OFF;
static volatile BaseType_t pumpMethaneInhibit = pdTRUE;

static BaseType_t PumpManager_IsEnvironmentSafe(void)
{
  SharedSensorData_t snapshot;

  osMutexAcquire(sensorDataMutexHandle, osWaitForever);
  snapshot = sharedSensorData;
  osMutexRelease(sensorDataMutexHandle);

  if (!snapshot.methaneValid || !snapshot.coValid || !snapshot.airFlowValid)
  {
    return pdFALSE;
  }

  if (snapshot.methaneLevel >= METHANE_CRITICAL_THRESHOLD)
  {
    return pdFALSE;
  }

  if (snapshot.coLevel >= CO_CRITICAL_THRESHOLD)
  {
    return pdFALSE;
  }

  /* Air flow alarms LOW, not high - insufficient ventilation is the fault. */
  if (snapshot.airFlowLevel <= AIRFLOW_LOW_THRESHOLD)
  {
    return pdFALSE;
  }

  return pdTRUE;
}

static void PumpManager_ApplyStateLocked(PumpState_t newState)
{
  if (pumpMethaneInhibit)
  {
    newState = PUMP_OFF;
  }

  HAL_GPIO_WritePin(PUMP_GPIO_Port, PUMP_Pin,
                     (newState == PUMP_ON) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  pumpCommandedState = newState;
}

void PumpManager_SetMethaneCritical(BaseType_t isCritical)
{
  osMutexAcquire(pumpMutexHandle, osWaitForever);

  pumpMethaneInhibit = isCritical;

  if (isCritical)
  {
    PumpManager_ApplyStateLocked(PUMP_OFF);
  }

  osMutexRelease(pumpMutexHandle);
}

static void PumpManager_SetPumpState(PumpState_t newState)
{
  osMutexAcquire(pumpMutexHandle, osWaitForever);
  PumpManager_ApplyStateLocked(newState);
  osMutexRelease(pumpMutexHandle);
}

void PumpManager_Toggle(void)
{
  osMutexAcquire(pumpMutexHandle, osWaitForever);
  PumpManager_ApplyStateLocked(pumpCommandedState == PUMP_OFF ? PUMP_ON : PUMP_OFF);
  osMutexRelease(pumpMutexHandle);
}

void PumpManagerTask_Run(void *argument) {
  uint16_t rawEvent;

#if DEBUG_UART_LOGGING
  char dbgBuf[80];
  int  dbgLen;
#endif

  for(;;)
  {
	  if (osMessageQueueGet(pumpCommandQueueHandle, &rawEvent, NULL, osWaitForever) != osOK)
	  {
		continue;
	  }

	  WaterLevelEvent_t evt = (WaterLevelEvent_t)rawEvent;

	  switch(evt){
	  case WATER_LEVEL_HIGH:
	  {
		  if(PumpManager_IsEnvironmentSafe())
		  {
			PumpManager_SetPumpState(PUMP_ON);
#if DEBUG_UART_LOGGING
          dbgLen = snprintf(dbgBuf, sizeof(dbgBuf),
                             "PumpManagerTask: HIGH + env OK -> pump ON\r\n");
#endif
		  }
		  else
		  {
		    PumpManager_SetPumpState(PUMP_OFF);
			/* No alarm raised here on purpose: PumpManager_IsEnvironmentSafe() can
				   return unsafe either because a sensor is actually critical (that
				   sensor's own task already called AlarmManager_RaiseCause the
				   moment it detected it) or because a reading isn't valid yet
				   (e.g. still warming up), which is not itself an alarm condition.
				   Duplicating the raise here would risk a false alarm on startup. */
#if DEBUG_UART_LOGGING
			dbgLen = snprintf(dbgBuf, sizeof(dbgBuf),
							   "PumpManagerTask: HIGH but env UNSAFE -> pump OFF\r\n");
#endif
		  }
		  break;
	  }
	  case WATER_LEVEL_LOW:
	  {
		  osMutexAcquire(pumpMutexHandle, osWaitForever);
		  PumpState_t current = pumpCommandedState;
		  osMutexRelease(pumpMutexHandle);

		  if(current == PUMP_ON) {
			  PumpManager_SetPumpState(PUMP_OFF);
		  }
#if DEBUG_UART_LOGGING
        dbgLen = snprintf(dbgBuf, sizeof(dbgBuf),
                           "PumpManagerTask: LOW -> pump OFF (was %s)\r\n",
                           (current == PUMP_ON) ? "ON" : "already OFF");
#endif
        break;
	  }
	  case WATER_LEVEL_NORMAL:
	  {
#if DEBUG_UART_LOGGING
        dbgLen = snprintf(dbgBuf, sizeof(dbgBuf),
                           "PumpManagerTask: NORMAL -> no action\r\n");
#endif
        break;
	  }
	  }

#if DEBUG_UART_LOGGING
    osMutexAcquire(uartLogMutexHandle, osWaitForever);
    HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
    osMutexRelease(uartLogMutexHandle);
#endif
  }
}
