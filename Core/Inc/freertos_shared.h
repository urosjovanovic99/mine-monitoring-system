/*
 * freertos_shared.h
 *
 * Shared declarations for the per-task source files (task_water_level,
 * task_methane, task_co_sensor, task_airflow, ...).
 *
 * The RTOS objects declared "extern" here are actually *defined* in
 * freertos.c. That file is CubeMX-generated outside of the USER CODE
 * markers, so the definitions themselves must stay there - regenerating
 * code from the .ioc will keep re-creating them. This header just gives
 * every task file a single place to pull those handles from instead of
 * re-declaring externs by hand in each one.
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_FREERTOS_SHARED_H_
#define INC_FREERTOS_SHARED_H_

#include "cmsis_os.h"
#include "sensor_adc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Set to 1 to enable per-sample UART debug logging from the sensor tasks. */
#define DEBUG_UART_LOGGING   1

/* RTOS objects created in MX_FREERTOS_Init() (freertos.c), used by task files. */
extern osMutexId_t        sensorDataMutexHandle;
extern osMutexId_t        uartLogMutexHandle;
extern osMessageQueueId_t pumpCommandQueueHandle;
extern osSemaphoreId_t    waterLevelSemaphoreHandle;

/* Latest readings from all sensor tasks, guarded by sensorDataMutexHandle. */
extern SharedSensorData_t sharedSensorData;

#ifdef __cplusplus
}
#endif

#endif /* INC_FREERTOS_SHARED_H_ */
