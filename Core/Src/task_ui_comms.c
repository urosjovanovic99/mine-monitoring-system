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
#include "task_water_level.h"
#include "sensor_fault.h"
#include "adc.h"
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include "perf_measure.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Map an armed ADC handle to a label for telemetry/UI. */
static const char *UIComms_FaultSensorName(void)
{
  ADC_HandleTypeDef *armed = SensorFault_ArmedTarget();
  if (armed == &hadc1) { return "METHANE"; }
  if (armed == &hadc2) { return "CO"; }
  if (armed == &hadc3) { return "AIRFLOW"; }
  return "NONE";
}

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
  /* ---- Simulation control (test/environment panel, not operator HMI) ---- */
  else if (strstr(line, "SIM_ON") != NULL)
  {
    simModeEnabled = 1U;
  }
  else if (strstr(line, "SIM_OFF") != NULL)
  {
    simModeEnabled = 0U;
  }
  else if (strstr(line, "SET_WATER_RATE") != NULL)
  {
    const char *p = strstr(line, "\"rate\":");
    if (p != NULL)
    {
      waterSimRate_mm_s = (int32_t)atoi(p + 7); /* signed: + fills, - drains */
    }
  }
  else if (strstr(line, "CRASH_SENSOR") != NULL)
  {
    /* Optional "count": defaults to 2 (crash twice -> alarm). */
    uint8_t count = SENSOR_FAULT_DEFAULT_COUNT;
    const char *c = strstr(line, "\"count\":");
    if (c != NULL)
    {
      count = (uint8_t)atoi(c + 8);
    }

    if (strstr(line, "METHANE") != NULL)      { SensorFault_Arm(&hadc1, count); }
    else if (strstr(line, "AIRFLOW") != NULL) { SensorFault_Arm(&hadc3, count); }
    else if (strstr(line, "CO") != NULL)      { SensorFault_Arm(&hadc2, count); }
  }
  /* ---- WCET measurement dump: prints per-task min/avg/max in us ---- */
  else if (strstr(line, "PERF_RESET") != NULL)
  {
    Perf_Reset();
  }
  else if (strstr(line, "PERF") != NULL)
  {
    char pbuf[512];
    int plen = Perf_FormatReport(pbuf, sizeof(pbuf));
    if (plen > 0)
    {
      osMutexAcquire(uartLogMutexHandle, osWaitForever);
      HAL_UART_Transmit(&huart2, (uint8_t *)pbuf, (uint16_t)plen, 200);
      osMutexRelease(uartLogMutexHandle);
    }
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

  alarmSnapshot = alarmCommandedState;

  char buf[320];
  int len = snprintf(buf, sizeof(buf),
      "{\"methane\":%u,\"methane_valid\":%u,"
      "\"co\":%u,\"co_valid\":%u,"
      "\"airflow\":%u,\"airflow_valid\":%u,"
      "\"waterflow\":%u,"
      "\"water_level\":%u,"
      "\"sim_mode\":%u,\"water_level_mm\":%d,\"water_rate\":%d,"
      "\"fault_sensor\":\"%s\","
      "\"pump\":%u,\"alarm\":%u}\r\n",
      (unsigned)snapshot.methaneLevel, (unsigned)snapshot.methaneValid,
      (unsigned)snapshot.coLevel, (unsigned)snapshot.coValid,
      (unsigned)snapshot.airFlowLevel, (unsigned)snapshot.airFlowValid,
	  (unsigned)waterFlowState,
	  (unsigned)waterLevelState,
	  (unsigned)simModeEnabled, (int)waterSimLevel_mm, (int)waterSimRate_mm_s,
	  UIComms_FaultSensorName(),
      (unsigned)pumpSnapshot, (unsigned)alarmSnapshot);

  if (len > 0)
  {
    osMutexAcquire(uartLogMutexHandle, osWaitForever);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, 100);
    osMutexRelease(uartLogMutexHandle);
  }
}

void UICommsTask_Run(void *argument)
{
  HAL_UART_Receive_IT(&huart2, &uiRxByte, 1); /* arm first RX byte once */

  char line[96];
#if PERF_MEASURE_ENABLE
  uint32_t perfStreamCtr = 0U;
#endif

  for (;;)
  {
    MEASURE_EXECUTION_TIME_BEGIN();
    UIComms_SendTelemetry();

    while (UIComms_TryReadLine(line, sizeof(line)))
    {
      UIComms_HandleLine(line);
    }

    MEASURE_EXECUTION_TIME_END(PERF_TASK_UICOMMS);

#if PERF_MEASURE_ENABLE
    /* Measurement build: stream the WCET table as its own JSON line every
     * PERF_STREAM_DIVIDER periods (~1 s). Sent AFTER the measurement window
     * above so this reporting overhead is not counted into UIComms' own C.
     * Uses a static buffer to keep it off this task's stack. */
    if (++perfStreamCtr >= PERF_STREAM_DIVIDER)
    {
      perfStreamCtr = 0U;
      static char perfLine[640];  /* headroom for large activation counts */
      int pn = Perf_FormatJson(perfLine, sizeof(perfLine));
      if (pn > 0)
      {
        osMutexAcquire(uartLogMutexHandle, osWaitForever);
        HAL_UART_Transmit(&huart2, (uint8_t *)perfLine, (uint16_t)pn, 200);
        osMutexRelease(uartLogMutexHandle);
      }
    }
#endif

    osDelay(UI_COMMS_TASK_PERIOD_MS);
  }
}
