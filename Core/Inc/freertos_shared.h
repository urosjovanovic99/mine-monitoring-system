/*
 * freertos_shared.h
 *
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

extern osMutexId_t        sensorDataMutexHandle;
extern osMutexId_t        uartLogMutexHandle;
extern osMessageQueueId_t pumpCommandQueueHandle;
extern osSemaphoreId_t    waterLevelSemaphoreHandle;
extern osMutexId_t 		  pumpMutexHandle;
extern osEventFlagsId_t	  alarmEventFlagsHandle;
extern SharedSensorData_t sharedSensorData;
extern volatile uint8_t simModeEnabled;

#ifdef __cplusplus
}
#endif

#endif /* INC_FREERTOS_SHARED_H_ */
