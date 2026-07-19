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

/* WaterLevelEvent_t is defined in main.h (WATER_LEVEL_NORMAL/HIGH/LOW). */

/**
 * @brief  Body of WaterLevelTask. Called from StartWaterLevelTask() in
 *         freertos.c.
 * @param  argument: Not used
 * @retval None (never returns)
 */
void WaterLevelTask_Run(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_TASK_WATER_LEVEL_H_ */
