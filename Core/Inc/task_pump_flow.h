/*
 * task_pump_flow.h
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_TASK_PUMP_FLOW_H_
#define INC_TASK_PUMP_FLOW_H_

#ifdef __cplusplus
extern "C" {
#endif

#define PUMP_FLOW_N_PERIODS      6      /* N: td = N * 150ms = 900 ms, per spec */
#define WATER_FLOW_ACTIVE_STATE  GPIO_PIN_SET

void PumpFlowTask_Run(void *argument);


#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_PUMP_FLOW_H_ */
