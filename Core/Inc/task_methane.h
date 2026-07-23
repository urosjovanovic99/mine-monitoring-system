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

#define METHANE_CRITICAL_THRESHOLD 1000
#define METHANE_TASK_PERIOD_MS     100U

void MethaneTask_Run(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_METHANE_H_ */
