/*
 * sensor_adc.h
 *
 *  Created on: Jul 18, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_SENSOR_ADC_H_
#define INC_SENSOR_ADC_H_

#include "adc.h"
#include "FreeRTOS.h"
#include "sim_env.h"   /* SimSensor_t, SIMULATION_BUILD */

/* USER CODE BEGIN Variables */
typedef struct
{
    uint16_t methaneLevel;
    uint16_t coLevel;
    uint16_t airFlowLevel;

    uint8_t  methaneValid;
    uint8_t  coValid;
    uint8_t  airFlowValid;
} SharedSensorData_t;

void ADC_HW_StartConversion(ADC_HandleTypeDef *hadc);
BaseType_t ADC_HW_ReadValue(ADC_HandleTypeDef *hadc, uint16_t *outValue);

/* Localized simulated-hardware read: identical to ADC_HW_ReadValue(), except
 * that in a SIMULATION_BUILD it first consults the fault-injection layer and,
 * when instructed, reports a status-register error (returns pdFALSE) without
 * reading the ADC. This is the single injection point for sensor faults - the
 * calling tasks stay unchanged apart from which read helper they invoke. */
BaseType_t Sensor_ReadWithFaults(SimSensor_t sensor, ADC_HandleTypeDef *hadc, uint16_t *outValue);

#endif /* INC_SENSOR_ADC_H_ */
