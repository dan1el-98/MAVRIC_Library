/**
 * \page The MAV'RIC License
 *
 * The MAV'RIC Framework
 *
 * Copyright � 2011-2014
 *
 * Laboratory of Intelligent Systems, EPFL
 */
 

/**
 * \file hud.h
 *
 *  This file sends the mavlink HUD message
 */


#ifndef HUD_H__
#define HUD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "position_estimation.h"
#include "stabilisation.h"
#include "imu.h"
#include "scheduler.h"

/**
 * \brief	The HUD structure to send the mavlink HUD message
 */ 
typedef struct  
{
	const position_estimator_t* pos_est;						///< The pointer to the position estimator structure
	const Control_Command_t* controls;							///< The pointer to the control structure
	const ahrs_t* ahrs;											///< The pointer to the attitude estimation structure
	const mavlink_stream_t* mavlink_stream;						///< The pointer to the mavlink stream structure
}hud_structure_t;

/**
 * \brief	Initialise the HUD structure
 * 
 * \param	hud_structure				///< The pointer to the HUD structure
 * \param	pos_est						///< The pointer to the position estimation structure
 * \param	controls					///< The pointer to the controls structure
 * \param	ahrs			///< The pointer to the attitude estimation structure
 * \param 	mavlink_stream				///< The pointer to the mavlink stream structure
 */
void hud_init(hud_structure_t *hud_structure, const position_estimator_t *pos_est, const Control_Command_t *controls, const ahrs_t *ahrs, const mavlink_stream_t* mavlink_stream);

/**
 * \brief	Task to send the mavlink HUD message
 * 
 * \param	hud_structure
 *
 * \return	The status of execution of the task
 */
task_return_t hud_send_message(hud_structure_t* hud_structure);

#ifdef __cplusplus
}
#endif

#endif //HUD_H__