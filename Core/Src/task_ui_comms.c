/*
 * task_ui_comms.c
 *
 *  Created on: Jul 21, 2026
 *      Author: uros.jovanovic
 */


#include "task_ui_comms.h"
#include "freertos_shared.h"
#include "task_alarm_manager.h"
#include "task_pump_manager.h"
#include "task_pump_flow.h"
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* ---- RX ring buffer, filled from HAL_UART_RxCpltCallback -------------- */
#define UI_RX_RING_SIZE   128
static uint8_t  uiRxRing[UI_RX_RING_SIZE];
static volatile uint16_t uiRxHead = 0;
static volatile uint16_t uiRxTail = 0;
static uint8_t  uiRxByte;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    uint16_t next = (uiRxHead + 1) % UI_RX_RING_SIZE;
    if (next != uiRxTail) /* drop byte on overflow rather than block in ISR */
    {
      uiRxRing[uiRxHead] = uiRxByte;
      uiRxHead = next;
    }
    HAL_UART_Receive_IT(&huart2, &uiRxByte, 1);
  }
}

/* Pulls one newline-terminated line out of the ring buffer. Non-blocking:
 * returns pdFALSE immediately if no complete line is available yet. */
static BaseType_t UIComms_TryReadLine(char *line, size_t maxLen)
{
  uint16_t idx = uiRxTail;
  size_t count = 0;

  while (idx != uiRxHead)
  {
    char c = (char)uiRxRing[idx];
    idx = (idx + 1) % UI_RX_RING_SIZE;

    if (c == '\n')
    {
      line[count] = '\0';
      uiRxTail = idx; /* commit consumption up to and including '\n' */
      return pdTRUE;
    }
    if (count < (maxLen - 1))
    {
      line[count++] = c;
    }
  }
  return pdFALSE;
}

static void UIComms_HandleLine(const char *line)
{
  if (strstr(line, "\"cmd\"") != NULL && strstr(line, "ALARM_ACK") != NULL)
  {
    AlarmManager_Acknowledge();
  }
  else if (strstr(line, "\"cmd\"") != NULL && strstr(line, "PUMP_TOGGLE") != NULL)
  {
    PumpManager_Toggle();
  }
  /* Unknown/partial commands are ignored - keep this task non-blocking. */
}

static void UIComms_SendTelemetry(void)
{
  SharedSensorData_t snapshot;
  PumpState_t pumpSnapshot;
  AlarmState_t alarmSnapshot;

  osMutexAcquire(sensorDataMutexHandle, osWaitForever);
  snapshot = sharedSensorData;
  osMutexRelease(sensorDataMutexHandle);

  osMutexAcquire(pumpMutexHandle, osWaitForever);
  pumpSnapshot = pumpCommandedState;
  osMutexRelease(pumpMutexHandle);

  /* alarmCommandedState has exactly one writer (AlarmManagerTask) and is a
   * word-sized read - same no-mutex assumption task_alarm_manager.c itself
   * already relies on for activeCauses. */
  alarmSnapshot = alarmCommandedState;

  char buf[160];
  int len = snprintf(buf, sizeof(buf),
      "{\"methane\":%u,\"methane_valid\":%u,"
      "\"co\":%u,\"co_valid\":%u,"
      "\"airflow\":%u,\"airflow_valid\":%u,"
      "\"waterflow\":%u,"
      "\"pump\":%u,\"alarm\":%u}\r\n",
      (unsigned)snapshot.methaneLevel, (unsigned)snapshot.methaneValid,
      (unsigned)snapshot.coLevel, (unsigned)snapshot.coValid,
      (unsigned)snapshot.airFlowLevel, (unsigned)snapshot.airFlowValid,
	  (unsigned)waterFlowState,
      (unsigned)pumpSnapshot, (unsigned)alarmSnapshot);

  if (len > 0)
  {
    /* Blocking transmit under uartLogMutexHandle, same as every other task's
     * debug output - deliberately NOT HAL_UART_Transmit_IT: mixing a
     * non-blocking IT transmit here with the blocking HAL_UART_Transmit
     * calls elsewhere would let a second task call HAL_UART_Transmit while
     * huart2 is still mid-transfer (HAL_UART_STATE_BUSY_TX), which returns
     * HAL_BUSY instead of actually sending. Serializing everyone on the
     * same mutex with the same (blocking) HAL call is what keeps this safe. */
    osMutexAcquire(uartLogMutexHandle, osWaitForever);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, 100);
    osMutexRelease(uartLogMutexHandle);
  }
}

void UICommsTask_Run(void *argument)
{
  HAL_UART_Receive_IT(&huart2, &uiRxByte, 1); /* arm first RX byte once */

  char line[96];

  for (;;)
  {
    UIComms_SendTelemetry();

    while (UIComms_TryReadLine(line, sizeof(line)))
    {
      UIComms_HandleLine(line);
    }

    osDelay(200);
  }
}
