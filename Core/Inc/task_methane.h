/*
 * task_methane.h
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_TASK_METHANE_H_
#define INC_TASK_METHANE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Body of MethaneTask. Called from StartMethaneSensorTask() in
 *         freertos.c.
 * @param  argument: Not used
 * @retval None (never returns)
 */
void MethaneTask_Run(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_METHANE_H_ */
