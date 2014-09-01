/** 
 * \page The MAV'RIC license
 *
 * The MAV'RIC Framework
 *
 * Copyright © 2011-2014
 *
 * Laboratory of Intelligent Systems, EPFL
 */
 
 
/**
 * \file stabilisation_copter->c
 *
 * This file handles the stabilization of the platform
 */


#include "stabilisation_copter.h"
#include "print_util.h"

void stabilisation_copter_init(stabilise_copter_t* stabilisation_copter, stabilise_copter_conf_t* stabiliser_conf, control_command_t* controls, const imu_t* imu, const ahrs_t* ahrs, const position_estimator_t* pos_est, servos_t* servos, mavlink_communication_t* mavlink_communication)
{
	
	stabilisation_copter->stabiliser_stack = stabiliser_conf->stabiliser_stack;
	stabilisation_copter->controls = controls;
	stabilisation_copter->imu = imu;
	stabilisation_copter->ahrs = ahrs;
	stabilisation_copter->pos_est = pos_est;
	stabilisation_copter->servos = servos;
	
	controls->control_mode = ATTITUDE_COMMAND_MODE;
	controls->yaw_mode = YAW_RELATIVE;
	
	controls->rpy[ROLL] = 0.0f;
	controls->rpy[PITCH] = 0.0f;
	controls->rpy[YAW] = 0.0f;
	controls->tvel[X] = 0.0f;
	controls->tvel[Y] = 0.0f;
	controls->tvel[Z] = 0.0f;
	controls->theading = 0.0f;
	controls->thrust = -1.0f;

	stabilisation_copter->stabiliser_stack.rate_stabiliser.mavlink_stream = &mavlink_communication->mavlink_stream;
	stabilisation_copter->stabiliser_stack.attitude_stabiliser.mavlink_stream = &mavlink_communication->mavlink_stream;
	stabilisation_copter->stabiliser_stack.velocity_stabiliser.mavlink_stream = &mavlink_communication->mavlink_stream;
	
	// Add callbacks for waypoint handler messages requests
	mavlink_message_handler_msg_callback_t callback;

	callback.message_id 	= MAVLINK_MSG_ID_MANUAL_CONTROL; // 69
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&stabilisation_copter_joystick_input;
	callback.module_struct 	= (handling_module_struct_t)		stabilisation_copter->controls;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );

	print_util_dbg_print("Stabilisation copter init.\r\n");
}

void stabilisation_copter_cascade_stabilise(stabilise_copter_t* stabilisation_copter)
{
	float rpyt_errors[4];
	control_command_t input;
	int32_t i;
	quat_t qtmp, q_rot;
	aero_attitude_t attitude_yaw_inverse;
	
	// set the controller input
	input= *stabilisation_copter->controls;
	switch (stabilisation_copter->controls->control_mode) {
	case VELOCITY_COMMAND_MODE:
		
		attitude_yaw_inverse = coord_conventions_quat_to_aero(stabilisation_copter->ahrs->qe);
		attitude_yaw_inverse.rpy[0] = 0.0f;
		attitude_yaw_inverse.rpy[1] = 0.0f;
		attitude_yaw_inverse.rpy[2] = attitude_yaw_inverse.rpy[2];
		
		//qtmp=quaternions_create_from_vector(input.tvel);
		//quat_t input_global = quaternions_local_to_global(stabilisation_copter->ahrs->qe, qtmp);
		
		q_rot = coord_conventions_quaternion_from_aero(attitude_yaw_inverse);
		
		quat_t input_global;
		quaternions_rotate_vector(q_rot, input.tvel, input_global.v);
		
		input.tvel[X] = input_global.v[X];
		input.tvel[Y] = input_global.v[Y];
		input.tvel[Z] = input_global.v[Z];
		
		rpyt_errors[X] = input.tvel[X] - stabilisation_copter->pos_est->vel[X];
		rpyt_errors[Y] = input.tvel[Y] - stabilisation_copter->pos_est->vel[Y];
		rpyt_errors[3] = -(input.tvel[Z] - stabilisation_copter->pos_est->vel[Z]);
		
		if (stabilisation_copter->controls->yaw_mode == YAW_COORDINATED) 
		{
			float rel_heading_coordinated;
			if ((maths_f_abs(stabilisation_copter->pos_est->vel_bf[X])<0.001f)&&(maths_f_abs(stabilisation_copter->pos_est->vel_bf[Y])<0.001f))
			{
				rel_heading_coordinated = 0.0f;
			}
			else
			{
				rel_heading_coordinated = atan2(stabilisation_copter->pos_est->vel_bf[Y], stabilisation_copter->pos_est->vel_bf[X]);
			}
			
			float w = 0.5f * (maths_sigmoid(vectors_norm(stabilisation_copter->pos_est->vel_bf)-stabilisation_copter->stabiliser_stack.yaw_coordination_velocity) + 1.0f);
			input.rpy[YAW] = (1.0f - w) * input.rpy[YAW] + w * rel_heading_coordinated;
		}

		rpyt_errors[YAW]= input.rpy[YAW];
		
		// run PID update on all velocity controllers
		stabilisation_run(&stabilisation_copter->stabiliser_stack.velocity_stabiliser, stabilisation_copter->imu->dt, rpyt_errors);
		
		//velocity_stabiliser.output.thrust = maths_f_min(velocity_stabiliser.output.thrust,stabilisation_param.controls->thrust);
		stabilisation_copter->stabiliser_stack.velocity_stabiliser.output.thrust += THRUST_HOVER_POINT;
		stabilisation_copter->stabiliser_stack.velocity_stabiliser.output.theading = input.theading;
		input = stabilisation_copter->stabiliser_stack.velocity_stabiliser.output;
		
		qtmp=quaternions_create_from_vector(stabilisation_copter->stabiliser_stack.velocity_stabiliser.output.rpy);
		//quat_t rpy_local = quaternions_global_to_local(stabilisation_copter->ahrs->qe, qtmp);
		
		quat_t rpy_local;
		quaternions_rotate_vector(quaternions_inverse(q_rot), qtmp.v, rpy_local.v);
		
		input.rpy[ROLL] = rpy_local.v[Y];
		input.rpy[PITCH] = -rpy_local.v[X];
		//input.thrust = stabilisation_copter->controls->tvel[Z];
		
	// -- no break here  - we want to run the lower level modes as well! -- 
	
	case ATTITUDE_COMMAND_MODE:
		// run absolute attitude_filter controller
		rpyt_errors[0]= input.rpy[0] - ( - stabilisation_copter->ahrs->up_vec.v[1] ); 
		rpyt_errors[1]= input.rpy[1] - stabilisation_copter->ahrs->up_vec.v[0];
		
		if ((stabilisation_copter->controls->yaw_mode == YAW_ABSOLUTE) ) {
			rpyt_errors[2] =maths_calc_smaller_angle(input.theading- stabilisation_copter->pos_est->local_position.heading);
		}
		else
		{ // relative yaw
			rpyt_errors[2]= input.rpy[2];
		}
		
		rpyt_errors[3]= input.thrust;       // no feedback for thrust at this level
		
		// run PID update on all attitude_filter controllers
		stabilisation_run(&stabilisation_copter->stabiliser_stack.attitude_stabiliser, stabilisation_copter->imu->dt, rpyt_errors);
		
		// use output of attitude_filter controller to set rate setpoints for rate controller 
		input = stabilisation_copter->stabiliser_stack.attitude_stabiliser.output;
	
	// -- no break here  - we want to run the lower level modes as well! -- 
	
	case RATE_COMMAND_MODE: // this level is always run
		// get rate measurements from IMU (filtered angular rates)
		for (i=0; i<3; i++)
		{
			rpyt_errors[i]= input.rpy[i]- stabilisation_copter->ahrs->angular_speed[i];
		}
		rpyt_errors[3] = input.thrust ;  // no feedback for thrust at this level
		
		// run PID update on all rate controllers
		stabilisation_run(&stabilisation_copter->stabiliser_stack.rate_stabiliser, stabilisation_copter->imu->dt, rpyt_errors);
	}
	
	// mix to servo outputs depending on configuration
	#ifdef CONF_DIAG
	stabilisation_copter_mix_to_servos_diag_quad(&stabilisation_copter->stabiliser_stack.rate_stabiliser.output, stabilisation_copter->servos);
	#else
	#ifdef CONF_CROSS
	stabilisation_copter_mix_to_servos_cross_quad(&stabilisation_copter->stabiliser_stack.rate_stabiliser.output, stabilisation_copter->servos);
	#endif
	#endif
}

void stabilisation_copter_mix_to_servos_diag_quad(control_command_t *control, servos_t* servos)
{
	int32_t i;
	float motor_command[4];
	
	motor_command[M_FRONT_RIGHT]= control->thrust + ( - control->rpy[ROLL] + control->rpy[PITCH]) + M_FR_DIR * control->rpy[YAW];
	motor_command[M_FRONT_LEFT] = control->thrust + ( control->rpy[ROLL] + control->rpy[PITCH]) + M_FL_DIR * control->rpy[YAW];
	motor_command[M_REAR_RIGHT] = control->thrust + ( - control->rpy[ROLL] - control->rpy[PITCH]) + M_RR_DIR * control->rpy[YAW];
	motor_command[M_REAR_LEFT]  = control->thrust + ( control->rpy[ROLL] - control->rpy[PITCH]) + M_RL_DIR * control->rpy[YAW];
	
	for (i=0; i<4; i++)
	{
		if (motor_command[i]<MIN_THRUST)
		{
			motor_command[i]=MIN_THRUST;
		}
		if (motor_command[i]>MAX_THRUST)
		{
			motor_command[i]=MAX_THRUST;
		}
	}

	for (i=0; i<4; i++) 
	{
		servos_set_value( servos, i, motor_command[i]);
	}
}

void stabilisation_copter_mix_to_servos_cross_quad(control_command_t *control, servos_t* servos)
{
	int32_t i;
	float motor_command[4];
	
	motor_command[M_FRONT]= control->thrust + control->rpy[PITCH] + M_FRONT_DIR * control->rpy[YAW];
	motor_command[M_RIGHT] = control->thrust - control->rpy[ROLL] + M_RIGHT_DIR * control->rpy[YAW];
	motor_command[M_REAR] = control->thrust - control->rpy[PITCH] + M_REAR_DIR * control->rpy[YAW];
	motor_command[M_LEFT]  = control->thrust + control->rpy[ROLL] + M_LEFT_DIR * control->rpy[YAW];
	for (i=0; i<4; i++) 
	{
		if (motor_command[i]<MIN_THRUST)
		{
			motor_command[i]=MIN_THRUST;
		}
		if (motor_command[i]>MAX_THRUST)
		{
			motor_command[i]=MAX_THRUST;
		}
	}
	for (i=0; i<4; i++) 
	{
		servos_set_value( servos, i, motor_command[i]);
	}
}

void stabilisation_copter_joystick_input(control_command_t *control, mavlink_received_t* rec)
{
	mavlink_manual_control_t packet;
	mavlink_msg_manual_control_decode(&rec->msg,&packet);
	
	if ((uint8_t)packet.target == (uint8_t)control->mavlink_stream->sysid)
	{
		print_util_dbg_print("Joystick command: (");
		print_util_dbg_print_num(packet.x,10);
		print_util_dbg_print(", ");
		print_util_dbg_print_num(packet.y,10);
		print_util_dbg_print(", ");
		print_util_dbg_print_num(packet.z,10);
		print_util_dbg_print("), ");
		print_util_dbg_print_num(packet.buttons,10);
		print_util_dbg_print(", ");
		print_util_dbg_print_num(packet.r,10);
		print_util_dbg_print("\r\n");
	}
}