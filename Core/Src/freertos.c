/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include "sensor_adc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
#define DEBUG_UART_LOGGING   1
SharedSensorData_t sharedSensorData;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for WaterLevelTask */
osThreadId_t WaterLevelTaskHandle;
const osThreadAttr_t WaterLevelTask_attributes = {
  .name = "WaterLevelTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for MethaneSensorTa */
osThreadId_t MethaneSensorTaHandle;
const osThreadAttr_t MethaneSensorTa_attributes = {
  .name = "MethaneSensorTa",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow6,
};
/* Definitions for COSensorTask */
osThreadId_t COSensorTaskHandle;
const osThreadAttr_t COSensorTask_attributes = {
  .name = "COSensorTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow5,
};
/* Definitions for pumpCommandQueue */
osMessageQueueId_t pumpCommandQueueHandle;
const osMessageQueueAttr_t pumpCommandQueue_attributes = {
  .name = "pumpCommandQueue"
};
/* Definitions for sensorDataMutex */
osMutexId_t sensorDataMutexHandle;
const osMutexAttr_t sensorDataMutex_attributes = {
  .name = "sensorDataMutex"
};
/* Definitions for waterLevelSemaphore */
osSemaphoreId_t waterLevelSemaphoreHandle;
const osSemaphoreAttr_t waterLevelSemaphore_attributes = {
  .name = "waterLevelSemaphore"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartWaterLevelTask(void *argument);
void StartMethaneSensorTask(void *argument);
void StartCOSensorTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of sensorDataMutex */
  sensorDataMutexHandle = osMutexNew(&sensorDataMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of waterLevelSemaphore */
  waterLevelSemaphoreHandle = osSemaphoreNew(1, 0, &waterLevelSemaphore_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of pumpCommandQueue */
  pumpCommandQueueHandle = osMessageQueueNew (16, sizeof(uint16_t), &pumpCommandQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of WaterLevelTask */
  WaterLevelTaskHandle = osThreadNew(StartWaterLevelTask, NULL, &WaterLevelTask_attributes);

  /* creation of MethaneSensorTa */
  MethaneSensorTaHandle = osThreadNew(StartMethaneSensorTask, NULL, &MethaneSensorTa_attributes);

  /* creation of COSensorTask */
  COSensorTaskHandle = osThreadNew(StartCOSensorTask, NULL, &COSensorTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartWaterLevelTask */
/**
* @brief Function implementing the WaterLevelTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartWaterLevelTask */
void StartWaterLevelTask(void *argument)
{
  /* USER CODE BEGIN StartWaterLevelTask */
  /* Infinite loop */
  for(;;)
  {
    if (osSemaphoreAcquire(waterLevelSemaphoreHandle, osWaitForever) == osOK)
    {
      GPIO_PinState highSet = HAL_GPIO_ReadPin(HIGH_WATER_GPIO_Port, HIGH_WATER_Pin);
      GPIO_PinState lowSet  = HAL_GPIO_ReadPin(LOW_WATER_GPIO_Port, LOW_WATER_Pin);

      WaterLevelEvent_t evt;
      const char *msg;
      if (highSet == GPIO_PIN_SET)
      {
        evt = WATER_LEVEL_HIGH;
        msg = "WaterLevelTask: HIGH -> pump ON\r\n";
      }
      else if (lowSet == GPIO_PIN_SET)
      {
        evt = WATER_LEVEL_LOW;
        msg = "WaterLevelTask: LOW -> pump OFF\r\n";
      }
      else
      {
        evt = WATER_LEVEL_NORMAL;
        msg = "WaterLevelTask: NORMAL (no action)\r\n";
      }

      /* TEMPORARY test-only output so the EXTI->task path is observable in
       * Renode. Remove or move into UICommsTask once real JSON telemetry is back. */
      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

      osMessageQueuePut(pumpCommandQueueHandle, &evt, 0, 0);
    }
  }
  /* USER CODE END StartWaterLevelTask */
}

/* USER CODE BEGIN Header_StartMethaneSensorTask */
/**
* @brief Function implementing the MethaneSensorTa thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartMethaneSensorTask */
void StartMethaneSensorTask(void *argument)
{
  /* USER CODE BEGIN StartMethaneSensorTask */
	TickType_t xLastWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS(150);
	uint16_t methaneValue;
	BaseType_t bConversionOk;
#if DEBUG_UART_LOGGING
	char dbgBuf[64];
	int  dbgLen;
#endif
	/* Prime the pipeline: kick off the very first conversion before
	   the periodic loop starts, so period 1's read has something
	   to read. */
	ADC_HW_StartConversion(&hadc1);

	xLastWakeTime = xTaskGetTickCount();
  /* Infinite loop */
	for (;;)
	{
		vTaskDelayUntil(&xLastWakeTime, xPeriod);

		/* This conversion was started >=150 ms ago (max latency is
		   50 ms) -> guaranteed complete. No polling needed for EOC;
		   SR is only consulted for the error bit. */
		bConversionOk = ADC_HW_ReadValue(&hadc1, &methaneValue);

		/* Immediately displace: start the conversion this reading's
		   "successor" will consume next period. */
		ADC_HW_StartConversion(&hadc1);

		if (bConversionOk)
		{
			osMutexAcquire(sensorDataMutexHandle, portMAX_DELAY);
			sharedSensorData.methaneLevel = methaneValue;
			sharedSensorData.methaneValid = pdTRUE;
			osMutexRelease(sensorDataMutexHandle);
#if DEBUG_UART_LOGGING
			dbgLen = snprintf(dbgBuf, sizeof(dbgBuf), "CH4 raw=%u tick=%lu\r\n",
                             methaneValue, (unsigned long)xLastWakeTime);
			HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
#endif
			if (methaneValue >= METHANE_CRITICAL_THRESHOLD)
			{
				// xSemaphoreGive(alarmEventSemaphore); turn alarm on
			}
		}
		else
		{
			/* device signaled an error in its status register —
			   feed into your "one bad reading tolerated" logic here */
			osMutexAcquire(sensorDataMutexHandle, osWaitForever);
			sharedSensorData.methaneValid = pdFALSE;
			osMutexRelease(sensorDataMutexHandle);
		}
	}
  /* USER CODE END StartMethaneSensorTask */
}

/* USER CODE BEGIN Header_StartCOSensorTask */
/**
* @brief Function implementing the COSensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCOSensorTask */
void StartCOSensorTask(void *argument)
{
  /* USER CODE BEGIN StartCOSensorTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCOSensorTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* USER CODE END Application */

