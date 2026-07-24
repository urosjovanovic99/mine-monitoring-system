/*
 * sensor_fault.c
 *
 *  Created on: Jul 24, 2026
 *      Author: uros.jovanovic
 */

#include "sensor_fault.h"
#include "freertos_shared.h"
#include "task.h"

static ADC_HandleTypeDef *volatile faultTarget    = NULL;
static volatile uint8_t            faultRemaining  = 0U;

void SensorFault_Arm(ADC_HandleTypeDef *hadc, uint8_t count)
{
  taskENTER_CRITICAL();
  faultTarget    = hadc;
  faultRemaining = count;
  taskEXIT_CRITICAL();
}

BaseType_t SensorFault_ShouldFail(ADC_HandleTypeDef *hadc)
{
  if (simModeEnabled && (faultRemaining > 0U) && (hadc == faultTarget))
  {
    faultRemaining--;
    return pdTRUE;
  }
  return pdFALSE;
}

ADC_HandleTypeDef *SensorFault_ArmedTarget(void)
{
  return (faultRemaining > 0U) ? faultTarget : NULL;
}
