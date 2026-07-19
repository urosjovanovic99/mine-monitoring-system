/*
 * sensor_adc.c
 *
 *  Created on: Jul 18, 2026
 *      Author: uros.jovanovic
 */
// sensor_adc.c
#include "sensor_adc.h"

void ADC_HW_StartConversion(ADC_HandleTypeDef *hadc)
{
    HAL_ADC_Start(hadc);
}

BaseType_t ADC_HW_ReadValue(ADC_HandleTypeDef *hadc, uint16_t *outValue)
{
    if (HAL_ADC_PollForConversion(hadc, 0) != HAL_OK)
    {
        return pdFALSE;
    }
    *outValue = (uint16_t)HAL_ADC_GetValue(hadc);
    return pdTRUE;
}

