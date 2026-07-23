/*
 * task_ui_comms.h
 *
 *  Created on: Jul 21, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_TASK_UI_COMMS_H_
#define INC_TASK_UI_COMMS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define UI_COMMS_TASK_PERIOD_MS     150U

void UICommsTask_Run(void *argument);


#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_UI_COMMS_H_ */
