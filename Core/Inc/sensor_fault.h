/*
 * sensor_fault.h
 *
 *  Created on: Jul 24, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_SENSOR_FAULT_H_
#define INC_SENSOR_FAULT_H_

#include "adc.h"
#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_FAULT_DEFAULT_COUNT  2U

void SensorFault_Arm(ADC_HandleTypeDef *hadc, uint8_t count);
BaseType_t SensorFault_ShouldFail(ADC_HandleTypeDef *hadc);

/* The handle with a fault still pending, or NULL. For UI/telemetry echo. */
ADC_HandleTypeDef *SensorFault_ArmedTarget(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_SENSOR_FAULT_H_ */
