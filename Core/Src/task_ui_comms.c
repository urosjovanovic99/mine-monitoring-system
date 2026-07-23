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
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include "sim_env.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

#ifdef SIMULATION_BUILD
/* Minimal JSON-value extractors. The UI emits flat, well-formed single-line
 * objects, so a full parser is overkill; these locate a "key" and read the
 * numeric literal that follows it. Return pdFALSE if the key is absent. */
static BaseType_t UIComms_FindLong(const char *line, const char *key, long *out)
{
  const char *p = strstr(line, key);
  if (p == NULL) { return pdFALSE; }
  p += strlen(key);
  while (*p != '\0' && *p != '-' && (*p < '0' || *p > '9')) { p++; }
  if (*p == '\0') { return pdFALSE; }
  *out = strtol(p, NULL, 10);
  return pdTRUE;
}

static BaseType_t UIComms_FindFloat(const char *line, const char *key, float *out)
{
  const char *p = strstr(line, key);
  if (p == NULL) { return pdFALSE; }
  p += strlen(key);
  while (*p != '\0' && *p != '-' && *p != '.' && (*p < '0' || *p > '9')) { p++; }
  if (*p == '\0') { return pdFALSE; }
  *out = strtof(p, NULL);
  return pdTRUE;
}

/* Map the "sensor" string value onto a SimSensor_t. Checks the longer,
 * unambiguous names before the "co" substring. */
static BaseType_t UIComms_FindSensor(const char *line, SimSensor_t *out)
{
  const char *p = strstr(line, "\"sensor\"");
  if (p == NULL) { return pdFALSE; }
  if (strstr(p, "methane") != NULL) { *out = SIM_SENSOR_METHANE; return pdTRUE; }
  if (strstr(p, "airflow") != NULL) { *out = SIM_SENSOR_AIRFLOW; return pdTRUE; }
  if (strstr(p, "co")      != NULL) { *out = SIM_SENSOR_CO;      return pdTRUE; }
  return pdFALSE;
}
#endif /* SIMULATION_BUILD */

static void UIComms_HandleLine(const char *line)
{
  if (strstr(line, "\"cmd\"") == NULL)
  {
    return; /* not a command frame */
  }

  if (strstr(line, "ALARM_ACK") != NULL)
  {
    AlarmManager_Acknowledge();
    return;
  }
  if (strstr(line, "PUMP_TOGGLE") != NULL)
  {
    PumpManager_Toggle();
    return;
  }

#ifdef SIMULATION_BUILD
  /* Interactive fault / timing injection from the operator console. */
  if (strstr(line, "SIM_FAIL_PCT") != NULL)
  {
    SimSensor_t sensor;
    long        pct;
    if (UIComms_FindSensor(line, &sensor) && UIComms_FindLong(line, "\"pct\"", &pct))
    {
      if (pct < 0)   { pct = 0; }
      if (pct > 100) { pct = 100; }
      Sim_SetFailPercent(sensor, (uint8_t)pct);
    }
    return;
  }
  if (strstr(line, "SIM_FORCE_FAIL") != NULL)
  {
    SimSensor_t sensor;
    long        n;
    if (UIComms_FindSensor(line, &sensor) && UIComms_FindLong(line, "\"n\"", &n))
    {
      if (n < 0)     { n = 0; }
      if (n > 65535) { n = 65535; }
      Sim_ForceFail(sensor, (uint16_t)n);
    }
    return;
  }
  if (strstr(line, "SIM_WATER_RATE") != NULL)
  {
    float fill, drain;
    if (UIComms_FindFloat(line, "\"fill\"", &fill) &&
        UIComms_FindFloat(line, "\"drain\"", &drain))
    {
      Sim_SetWaterRates(fill, drain);
    }
    return;
  }
#endif /* SIMULATION_BUILD */

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

#ifdef SIMULATION_BUILD
  unsigned tankPct = (unsigned)Sim_TankLevelPct();
#else
  unsigned tankPct = 0u;
#endif

  char buf[224];
  int len = snprintf(buf, sizeof(buf),
      "{\"methane\":%u,\"methane_valid\":%u,"
      "\"co\":%u,\"co_valid\":%u,"
      "\"airflow\":%u,\"airflow_valid\":%u,"
      "\"waterflow\":%u,"
      "\"water_level\":%u,"
      "\"tank_level\":%u,"
      "\"pump\":%u,\"alarm\":%u}\r\n",
      (unsigned)snapshot.methaneLevel, (unsigned)snapshot.methaneValid,
      (unsigned)snapshot.coLevel, (unsigned)snapshot.coValid,
      (unsigned)snapshot.airFlowLevel, (unsigned)snapshot.airFlowValid,
	  (unsigned)waterFlowState,
	  (unsigned)waterLevelState,
	  tankPct,
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

    osDelay(UI_COMMS_TASK_PERIOD_MS);
  }
}
