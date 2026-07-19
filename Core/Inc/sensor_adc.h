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

#define METHANE_CRITICAL_THRESHOLD 1000
#define CO_CRITICAL_THRESHOLD 1000


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

#endif /* INC_SENSOR_ADC_H_ */
