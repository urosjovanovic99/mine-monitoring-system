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
#include "freertos_shared.h"
#include "task_water_level.h"
#include "task_methane.h"
#include "task_co_sensor.h"
#include "task_airflow.h"
#include "task_pump_flow.h"
#include "task_pump_manager.h"
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
/* Definition lives here because MX_FREERTOS_Init() and the RTOS object
 * handles below are CubeMX-owned; every task file pulls this in via
 * freertos_shared.h (extern SharedSensorData_t sharedSensorData;). */
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
  .priority = (osPriority_t) osPriorityLow7,
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
/* Definitions for PumpFlowTask */
osThreadId_t PumpFlowTaskHandle;
const osThreadAttr_t PumpFlowTask_attributes = {
  .name = "PumpFlowTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow6,
};
/* Definitions for PumpManagerTask */
osThreadId_t PumpManagerTaskHandle;
const osThreadAttr_t PumpManagerTask_attributes = {
  .name = "PumpManagerTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow2,
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
/* Definitions for pumpMutex */
osMutexId_t pumpMutexHandle;
const osMutexAttr_t pumpMutex_attributes = {
  .name = "pumpMutex"
};
/* Definitions for waterLevelSemaphore */
osSemaphoreId_t waterLevelSemaphoreHandle;
const osSemaphoreAttr_t waterLevelSemaphore_attributes = {
  .name = "waterLevelSemaphore"
};
/* Definitions for alarmEventSemaphore */
osSemaphoreId_t alarmEventSemaphoreHandle;
const osSemaphoreAttr_t alarmEventSemaphore_attributes = {
  .name = "alarmEventSemaphore"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartWaterLevelTask(void *argument);
void StartMethaneSensorTask(void *argument);
void StartCOSensorTask(void *argument);
void StartAirFlowSensorTask(void *argument);
void StartPumpFlowTask(void *argument);
void StartPumpManagerTask(void *argument);

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

  /* creation of pumpMutex */
  pumpMutexHandle = osMutexNew(&pumpMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of waterLevelSemaphore */
  waterLevelSemaphoreHandle = osSemaphoreNew(1, 0, &waterLevelSemaphore_attributes);

  /* creation of alarmEventSemaphore */
  alarmEventSemaphoreHandle = osSemaphoreNew(1, 0, &alarmEventSemaphore_attributes);

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

  /* creation of PumpFlowTask */
  PumpFlowTaskHandle = osThreadNew(StartPumpFlowTask, NULL, &PumpFlowTask_attributes);

  /* creation of PumpManagerTask */
  PumpManagerTaskHandle = osThreadNew(StartPumpManagerTask, NULL, &PumpManagerTask_attributes);

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
  WaterLevelTask_Run(argument);
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
  MethaneTask_Run(argument);
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
  COSensorTask_Run(argument);
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
  AirFlowTask_Run(argument);
  /* USER CODE END StartAirFlowSensorTask */
}

/* USER CODE BEGIN Header_StartPumpFlowTask */
/**
* @brief Function implementing the PumpFlowTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartPumpFlowTask */
void StartPumpFlowTask(void *argument)
{
  /* USER CODE BEGIN StartPumpFlowTask */
  PumpFlowTask_Run(argument);
  /* USER CODE END StartPumpFlowTask */
}

/* USER CODE BEGIN Header_StartPumpManagerTask */
/**
* @brief Function implementing the PumpManagerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartPumpManagerTask */
void StartPumpManagerTask(void *argument)
{
  /* USER CODE BEGIN StartPumpManagerTask */
  PumpManagerTask_Run(argument);
  /* USER CODE END StartPumpManagerTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* USER CODE END Application */

