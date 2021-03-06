/*******************************************************************************
 * Copyright (c) 2009-2016, MAV'RIC Development Team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

/*******************************************************************************
 * \file flight_controller_quadcopter.hpp
 *
 * \author MAV'RIC Team
 * \author Julien Lecoeur
 *
 * \brief   Full flight controller for quadcopter
 *
 ******************************************************************************/

#ifndef FLIGHT_CONTROLLER_QUADCOPTER_HPP_
#define FLIGHT_CONTROLLER_QUADCOPTER_HPP_

#include "flight_controller/flight_controller_copter.hpp"

class Flight_controller_quadcopter: public Flight_controller_copter<4>
{
public:
    Flight_controller_quadcopter(const INS& ins, const AHRS& ahrs, Servo& motor_rl, Servo& motor_fl, Servo& motor_fr, Servo& motor_rr, conf_t config):
        Flight_controller_copter<4>(ins, ahrs, Servos_mix_matrix<4>::args_t{{{&motor_rl, &motor_fl, &motor_fr, &motor_rr}}}, config)
    {};

    static conf_t default_config(void)
    {
        return default_config_diag();
    };

    static conf_t default_config_diag(void)
    {
        conf_t conf = Flight_controller_copter<4>::default_config();
        //                                 roll, pitch,   yaw,   X,    Y,     Z
        conf.mix_config.mix  = Mat<4, 6>({ 1.0f, -1.0f,  1.0f, 0.0f, 0.0f, -1.0f,   // rear left
                                           1.0f,  1.0f, -1.0f, 0.0f, 0.0f, -1.0f,   // front left
                                          -1.0f,  1.0f,  1.0f, 0.0f, 0.0f, -1.0f,   // front right
                                          -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f}); // rear right

        return conf;
    };

    static conf_t default_config_cross(void)
    {
        conf_t conf = Flight_controller_copter<4>::default_config();
        //                                 roll, pitch,   yaw,   X,    Y,     Z
        conf.mix_config.mix  = Mat<4, 6>({ 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f,    // rear
                                           1.0f,  0.0f,  1.0f, 0.0f, 0.0f, -1.0f,    // left
                                           0.0f,  1.0f, -1.0f, 0.0f, 0.0f, -1.0f,    // front
                                          -1.0f,  0.0f,  1.0f, 0.0f, 0.0f, -1.0f }); // right

        return conf;
    };
};

#endif  // FLIGHT_CONTROLLER_QUADCOPTER_HPP_
