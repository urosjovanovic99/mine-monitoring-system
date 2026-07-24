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

#define WATER_SIM_TICK_MS                100U
#define WATER_SIM_MAX_MM                 1000
#define WATER_SIM_HIGH_MM                800
#define WATER_SIM_LOW_MM                 200
#define WATER_SIM_START_MM               400

extern volatile WaterLevelEvent_t waterLevelState;
extern volatile int32_t waterSimRate_mm_s;
extern volatile int32_t waterSimLevel_mm;

void WaterLevelTask_Run(void *argument);
void WaterSim_TimerCb(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_WATER_LEVEL_H_ */
