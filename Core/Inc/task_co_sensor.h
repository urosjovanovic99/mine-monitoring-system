/*
 * task_co_sensor.h
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_TASK_CO_SENSOR_H_
#define INC_TASK_CO_SENSOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CO_CRITICAL_THRESHOLD 1000
/**
 * @brief  Body of COSensorTask. Called from StartCOSensorTask() in
 *         freertos.c.
 * @param  argument: Not used
 * @retval None (never returns)
 */
void COSensorTask_Run(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_CO_SENSOR_H_ */
