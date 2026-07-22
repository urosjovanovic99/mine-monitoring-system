/*
 * task_pump_manager.h
 *
 *  Created on: Jul 20, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_TASK_PUMP_MANAGER_H_
#define INC_TASK_PUMP_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { PUMP_OFF = 0, PUMP_ON = 1 } PumpState_t;
extern PumpState_t pumpCommandedState;

void PumpManager_Toggle(void);
void PumpManagerTask_Run(void *argument);


#ifdef __cplusplus
}
#endif


#endif /* INC_TASK_PUMP_MANAGER_H_ */
