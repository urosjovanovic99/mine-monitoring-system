/*
 * task_airflow.h
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_TASK_AIRFLOW_H_
#define INC_TASK_AIRFLOW_H_

#ifdef __cplusplus
extern "C" {
#endif

#define AIRFLOW_LOW_THRESHOLD 1000

/**
 * @brief  Body of AirFlowTask. Called from StartAirFlowSensorTask() in
 *         freertos.c.
 * @param  argument: Not used
 * @retval None (never returns)
 */
void AirFlowTask_Run(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_AIRFLOW_H_ */
