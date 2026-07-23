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

BaseType_t Sensor_ReadWithFaults(SimSensor_t sensor, ADC_HandleTypeDef *hadc, uint16_t *outValue)
{
#ifdef SIMULATION_BUILD
    if (Sim_ShouldFailRead(sensor))
    {
        /* Emulate a device that raised the error bit in its status register:
         * behave exactly as ADC_HW_ReadValue() does on a real conversion
         * error - report failure and leave *outValue untouched. */
        return pdFALSE;
    }
#endif
    return ADC_HW_ReadValue(hadc, outValue);
}

