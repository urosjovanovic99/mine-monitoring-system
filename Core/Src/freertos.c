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
  .priority = (osPriority_t) osPriorityLow3,
};
/* Definitions for MethaneTask */
osThreadId_t MethaneTaskHandle;
const osThreadAttr_t MethaneTask_attributes = {
  .name = "MethaneTask",
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
/* Definitions for AirFlowTask */
osThreadId_t AirFlowTaskHandle;
const osThreadAttr_t AirFlowTask_attributes = {
  .name = "AirFlowTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow4,
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
/* Definitions for uartLogMutex */
osMutexId_t uartLogMutexHandle;
const osMutexAttr_t uartLogMutex_attributes = {
  .name = "uartLogMutex"
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
void StartAirFlowSensorTask(void *argument);

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

  /* creation of uartLogMutex */
  uartLogMutexHandle = osMutexNew(&uartLogMutex_attributes);

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

  /* creation of MethaneTask */
  MethaneTaskHandle = osThreadNew(StartMethaneSensorTask, NULL, &MethaneTask_attributes);

  /* creation of COSensorTask */
  COSensorTaskHandle = osThreadNew(StartCOSensorTask, NULL, &COSensorTask_attributes);

  /* creation of AirFlowTask */
  AirFlowTaskHandle = osThreadNew(StartAirFlowSensorTask, NULL, &AirFlowTask_attributes);

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
			osMutexAcquire(uartLogMutexHandle, osWaitForever);
			dbgLen = snprintf(dbgBuf, sizeof(dbgBuf), "CH4 raw=%u tick=%lu\r\n",
                             methaneValue, (unsigned long)xLastWakeTime);
			HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
			osMutexRelease(uartLogMutexHandle);
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
	TickType_t xLastWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS(150);
	uint16_t coValue;
	BaseType_t bConversionOk;

#if DEBUG_UART_LOGGING
	char dbgBuf[64];
	int  dbgLen;
#endif

	ADC_HW_StartConversion(&hadc2);

	xLastWakeTime = xTaskGetTickCount();
	for (;;)
	{
	  vTaskDelayUntil(&xLastWakeTime, xPeriod);

	  bConversionOk = ADC_HW_ReadValue(&hadc2, &coValue);
	  ADC_HW_StartConversion(&hadc2);

	  if (bConversionOk)
	  {
		  osMutexAcquire(sensorDataMutexHandle, osWaitForever);
		  sharedSensorData.coLevel = coValue;
		  sharedSensorData.coValid = pdTRUE;
		  osMutexRelease(sensorDataMutexHandle);

#if DEBUG_UART_LOGGING
		  osMutexAcquire(uartLogMutexHandle, osWaitForever);
		  dbgLen = snprintf(dbgBuf, sizeof(dbgBuf), "CO raw=%u tick=%lu\r\n",
							 coValue, (unsigned long)xLastWakeTime);
		  HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
		  osMutexRelease(uartLogMutexHandle);
#endif

		  if (coValue >= CO_CRITICAL_THRESHOLD)
		  {
			  // osSemaphoreRelease(alarmEventSemaphoreHandle); turn alarm on
		  }
	  }
	  else
	  {
		  osMutexAcquire(sensorDataMutexHandle, osWaitForever);
		  sharedSensorData.coValid = pdFALSE;
		  osMutexRelease(sensorDataMutexHandle);
	  }
  }
  /* USER CODE END StartCOSensorTask */
}

/* USER CODE BEGIN Header_StartAirFlowSensorTask */
/**
* @brief Function implementing the AirFlowSensorTa thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartAirFlowSensorTask */
void StartAirFlowSensorTask(void *argument)
{
  /* USER CODE BEGIN StartAirFlowSensorTask */
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(150);
  uint16_t airFlowValue;
  BaseType_t bConversionOk;
  uint8_t consecutiveErrors = 0;

#if DEBUG_UART_LOGGING
  char dbgBuf[64];
  int  dbgLen;
#endif

  ADC_HW_StartConversion(&hadc3);

  xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
	  vTaskDelayUntil(&xLastWakeTime, xPeriod);

	  bConversionOk = ADC_HW_ReadValue(&hadc3, &airFlowValue);
	  ADC_HW_StartConversion(&hadc3);

	  if (bConversionOk)
	  {
		  consecutiveErrors = 0;

		  osMutexAcquire(sensorDataMutexHandle, osWaitForever);
		  sharedSensorData.airFlowLevel = airFlowValue;
		  sharedSensorData.airFlowValid = pdTRUE;
		  osMutexRelease(sensorDataMutexHandle);

#if DEBUG_UART_LOGGING
		  osMutexAcquire(uartLogMutexHandle, osWaitForever);
		  dbgLen = snprintf(dbgBuf, sizeof(dbgBuf), "AIRFLOW raw=%u tick=%lu\r\n",
							 airFlowValue, (unsigned long)xLastWakeTime);
		  HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
		  osMutexRelease(uartLogMutexHandle);
#endif

		  /* Air flow alarms on too LOW a reading, not too high -
			 insufficient ventilation is the fault condition here. */
		  if (airFlowValue <= AIRFLOW_LOW_THRESHOLD)
		  {
			  // osSemaphoreRelease(alarmEventSemaphoreHandle); alarm should turn on
		  }
	  }
	  else
	  {   // error recovery still have to analyze this part of code
		  osMutexAcquire(sensorDataMutexHandle, osWaitForever);
		  sharedSensorData.airFlowValid = pdFALSE;
		  osMutexRelease(sensorDataMutexHandle);

		  if (consecutiveErrors < 0xFF)  /* guard against wraparound */
		  {
			  consecutiveErrors++;
		  }

		  /* Spec requirement: 2 consecutive read errors -> alarm */
		  if (consecutiveErrors >= 2)
		  {
			  // osSemaphoreRelease(alarmEventSemaphoreHandle); turn on alarm
		  }

#if DEBUG_UART_LOGGING
		  osMutexAcquire(uartLogMutexHandle, osWaitForever);
		  dbgLen = snprintf(dbgBuf, sizeof(dbgBuf), "AIRFLOW ERROR (%u consecutive) tick=%lu\r\n",
							 consecutiveErrors, (unsigned long)xLastWakeTime);
		  HAL_UART_Transmit(&huart2, (uint8_t *)dbgBuf, dbgLen, 100);
		  osMutexRelease(uartLogMutexHandle);
#endif
	      }
	  }
  /* USER CODE END StartAirFlowSensorTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* USER CODE END Application */

