/*
 * task_water_level.h
 *
 *  Created on: Jul 19, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_TASK_WATER_LEVEL_H_
#define INC_TASK_WATER_LEVEL_H_

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WATER_LEVEL_DEADLINE_MS          200U
#define WATER_LEVEL_MIN_INTERARRIVAL_MS  5000U
#define WATER_LEVEL_DEBOUNCE_MS          50U

/* Latest accepted water level. Single writer (WaterLevelTask), read without a
 * mutex by task_ui_comms.c - same word-sized single-writer assumption already
 * relied on for waterFlowState. */
extern volatile WaterLevelEvent_t waterLevelState;

void WaterLevelTask_Run(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_WATER_LEVEL_H_ */
