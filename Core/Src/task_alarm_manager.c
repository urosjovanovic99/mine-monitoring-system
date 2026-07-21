/*
 * task_alarm_manager.c
 *
 *  Created on: Jul 20, 2026
 *      Author: uros.jovanovic
 */

#include "task_alarm_manager.h"
#include "freertos_shared.h"
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>

AlarmState_t alarmCommandedState = ALARM_OFF;

/* Only AlarmManagerTask_Run() writes this -> plain volatile is enough,
 * no mutex needed (single writer, word-aligned reads are atomic on
 * Cortex-M4). */
static volatile uint32_t activeCauses = 0;

void AlarmManager_RaiseCause(uint32_t causeBit)
{
  osEventFlagsSet(alarmEventFlagsHandle, causeBit & ALARM_CAUSE_MASK);
}

void AlarmManager_Acknowledge(void)
{
  osEventFlagsSet(alarmEventFlagsHandle, ALARM_BIT_ACK);
}

void AlarmManagerTask_Run(void *argument)
{
#if DEBUG_UART_LOGGING
  char dbgBuf[64];
  int  dbgLen;
#endif
  for (;;)
  {
    /* osEventFlagsWait() clears the matched bits on return by default
     * (osFlagsNoClear not passed) -> this blocks until a NEW SetBits()
     * call happens. It does not busy-loop even though the alarm is
     * really "level" state, because activeCauses folds each arrival
     * into a persistent bitmask below. */
    uint32_t bits = osEventFlagsWait(alarmEventFlagsHandle, ALARM_ALL_BITS,
                                      osFlagsWaitAny, osWaitForever);

    if (bits & osFlagsError)
    {
      continue; /* real error only - osWaitForever can't time out */
    }

    if (bits & ALARM_BIT_ACK)
    {
      /* Operator reset. Safety-first: if a genuine new cause bit
       * arrived in this same batch, keep it - don't let an ack
       * silently swallow a fresh hazard. */
      activeCauses = bits & ALARM_CAUSE_MASK;
    }
    else
    {
      activeCauses |= (bits & ALARM_CAUSE_MASK);
    }

    AlarmState_t shouldBe = (activeCauses != 0) ? ALARM_ON : ALARM_OFF;
    if (shouldBe != alarmCommandedState)
    {
      HAL_GPIO_WritePin(ALARM_GPIO_Port, ALARM_Pin,
                         (shouldBe == ALARM_ON) ? GPIO_PIN_SET : GPIO_PIN_RESET);
      alarmCommandedState = shouldBe;
#if DEBUG_UART_LOGGING
      osMutexAcquire(uartLogMutexHandle, osWaitForever);
      dbgLen = snprintf(dbgBuf, sizeof(dbgBuf), (shouldBe == ALARM_ON) ? "ALARM IS ON\n" : "ALARM IS OFF\n");
      HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
      osMutexRelease(uartLogMutexHandle);
#endif
    }
  }
}
