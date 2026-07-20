/*
 * task_alarm_manager.h
 *
 *  Created on: Jul 20, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_TASK_ALARM_MANAGER_H_
#define INC_TASK_ALARM_MANAGER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { ALARM_OFF = 0, ALARM_ON = 1 } AlarmState_t;
extern AlarmState_t alarmCommandedState;

/* One bit per independent hazard source. Producers only ever SET their
 * own bit, on the rising edge of their own critical condition - the
 * alarm latches, it is never cleared by the source that raised it.
 * ALARM_BIT_ACK is not a cause, it is the operator's acknowledge/reset
 * trigger and the only thing allowed to clear the group. */
#define ALARM_BIT_METHANE    (1UL << 0)
#define ALARM_BIT_CO         (1UL << 1)
#define ALARM_BIT_AIRFLOW    (1UL << 2)
#define ALARM_BIT_PUMPFAULT  (1UL << 3)
#define ALARM_BIT_ACK        (1UL << 4)

#define ALARM_CAUSE_MASK     (ALARM_BIT_METHANE | ALARM_BIT_CO | ALARM_BIT_AIRFLOW | ALARM_BIT_PUMPFAULT)
#define ALARM_ALL_BITS       (ALARM_CAUSE_MASK | ALARM_BIT_ACK)

/**
 * @brief  Body of AlarmManagerTask. Called from StartAlarmManageTask() in
 *         freertos.c.
 * @param  argument: Not used
 * @retval None (never returns)
 */
void AlarmManagerTask_Run(void *argument);

/**
 * @brief  Called by MethaneTask / COSensorTask / AirFlowTask / PumpFlowTask,
 *         ONLY on the rising edge of their own critical condition (they
 *         already track their previous state for hysteresis / the
 *         2-consecutive-read-error rule, so this costs them nothing extra).
 *         Safe to call from a task or an ISR (configUSE_OS2_EVENTFLAGS_FROM_ISR
 *         is already enabled in FreeRTOSConfig.h).
 */
void AlarmManager_RaiseCause(uint32_t causeBit);

/**
 * @brief  Operator acknowledge/reset - the only thing that turns the alarm
 *         back off. Call this from wherever the operator command arrives
 *         (UICommsTask parsing an "ALARM_ACK" command once it exists, or a
 *         button ISR for local testing in the meantime).
 */
void AlarmManager_Acknowledge(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_ALARM_MANAGER_H_ */
