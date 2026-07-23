/*
 * sim_env.h
 *
 * Interactive simulation / test-harness layer (Zadatak 2 requirement:
 * "interaktivna simulacija sa mogucnoscu zadavanja vremenskih karakteristika
 *  i vanrednih situacija").
 *
 * This module is the software model of the *environment and the I/O devices*
 * that, on real hardware, the controller would talk to. Per the assignment,
 * all interactions with I/O devices are localized in dedicated procedures that
 * "simulate the operation of real hardware"; this file is where that
 * simulation and its runtime-configurable fault behavior live, so the control
 * tasks themselves stay unaware they are being tested.
 *
 * Two capabilities are exposed to the operator console (PyQt) over the same
 * USART2 command channel already used for PUMP_TOGGLE / ALARM_ACK:
 *   1. Sensor read-failure injection (probabilistic percent + deterministic
 *      "force next N reads to fail") for the methane / CO / airflow ADC reads.
 *   2. A water-tank plant model with an operator-settable fill/drain rate that
 *      drives the HIGH / LOW water-level events.
 *
 *  Created on: Jul 24, 2026
 *      Author: uros.jovanovic
 */

#ifndef INC_SIM_ENV_H_
#define INC_SIM_ENV_H_

/* ------------------------------------------------------------------------ *
 * Master switch for ALL test scaffolding in this project.
 *
 * Defined  -> fault injection + water-tank plant model are compiled in and
 *             driven interactively from the UI.
 * Commented -> the firmware builds as the pure "real" controller: sensor reads
 *             go straight to the ADC, water level comes from the physical
 *             HIGH/LOW EXTI detectors, and there is ZERO simulation overhead.
 *             Use this configuration as the WCET / schedulability baseline for
 *             Zadatak 3.
 * ------------------------------------------------------------------------ */
#define SIMULATION_BUILD 1

#include "FreeRTOS.h"
#include "main.h"               /* WaterLevelEvent_t */
#include "task_pump_manager.h"  /* PumpState_t       */

#ifdef __cplusplus
extern "C" {
#endif

/* Sensors that support read-failure injection. Order is fixed: the UI sends a
 * sensor name string and it is mapped onto these indices. */
typedef enum
{
    SIM_SENSOR_METHANE = 0,
    SIM_SENSOR_CO,
    SIM_SENSOR_AIRFLOW,
    SIM_SENSOR_COUNT
} SimSensor_t;

/* Water-tank plant model, expressed as a percentage of full capacity.
 * Crossing HIGH -> WATER_LEVEL_HIGH event; crossing LOW -> WATER_LEVEL_LOW. */
#define SIM_TANK_HIGH_PCT   80.0f
#define SIM_TANK_LOW_PCT    20.0f
#define SIM_TANK_STEP_MS    100U    /* plant integration period */

/* Called once from MX_FREERTOS_Init() before the scheduler starts. */
void Sim_Init(void);

/* ---- sensor read-failure injection ------------------------------------- *
 * Sim_ShouldFailRead() is consulted by the localized ADC read procedure. It
 * returns pdTRUE when this particular read must be reported as a status-
 * register error (emulating a device that failed its conversion). "Force N"
 * failures are consumed first (deterministic, for demonstrating the
 * two-consecutive-errors -> alarm rule on demand), then the probabilistic
 * percentage is applied. */
BaseType_t Sim_ShouldFailRead(SimSensor_t sensor);
void       Sim_SetFailPercent(SimSensor_t sensor, uint8_t percent);   /* 0..100 */
void       Sim_ForceFail(SimSensor_t sensor, uint16_t count);

/* ---- water-tank plant model -------------------------------------------- */
void              Sim_SetWaterRates(float fillPctPerSec, float drainPctPerSec);
void              Sim_TankStep(uint32_t dtMs, PumpState_t pump);
WaterLevelEvent_t Sim_TankClassify(void);
uint8_t           Sim_TankLevelPct(void);   /* rounded level, for telemetry */

/* Periodic plant task: integrates the tank level and releases
 * waterLevelSemaphoreHandle on every threshold crossing, exactly as the EXTI
 * ISR does for the physical detectors. */
void SimTankTask_Run(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_SIM_ENV_H_ */
