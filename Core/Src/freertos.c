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
/* Definitions for pumpCommandQueue */
osMessageQueueId_t pumpCommandQueueHandle;
const osMessageQueueAttr_t pumpCommandQueue_attributes = {
  .name = "pumpCommandQueue"
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

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

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

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */