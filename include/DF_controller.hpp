/*!********************************************************************************
 * \brief     Differential Flatness controller Implementation
 * \authors   Miguel Fernandez-Cortizas
 * \copyright Copyright (c) 2020 Universidad Politecnica de Madrid
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/

#ifndef __PD_CONTROLLER_H__
#define __PD_CONTROLLER_H__

// Std libraries
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <rclcpp/logging.hpp>
#include <unordered_map>
#include <vector>

// Eigen
#include <Eigen/Dense>

#include "Eigen/src/Core/Matrix.h"
#include "as2_control_command_handlers/acro_control.hpp"
#include "as2_core/node.hpp"
#include "as2_core/names/topics.hpp"
#include "as2_msgs/msg/thrust.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.h"
#include "trajectory_msgs/msg/joint_trajectory_point.hpp"

// FIXME: read this from the parameter server

#define SATURATE_YAW_ERROR 1

#define SPEED_REFERENCE 0

using Vector3d = Eigen::Vector3d;

struct Control_flags {
  bool traj_generated;
  bool hover_position;
  bool state_received;
};

struct UAV_state {
  // State composed of s = [pose ,d_pose]'
  Vector3d pos;
  Vector3d rot;
  Vector3d vel;
};

class PD_controller : public as2::Node {
  private:
  float mass = 1.5f;

  rclcpp::Subscription<trajectory_msgs::msg::JointTrajectoryPoint>::SharedPtr sub_traj_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;

  UAV_state state_;
  Control_flags flags_;

  // controller stuff
  const float g = 9.81;

  // Eigen::Vector3d Kp_lin_;
  // Eigen::Vector3d Kd_lin_;
  // Eigen::Vector3d Kp_ang_;
  // Eigen::Vector3d Ki_lin_;
  Eigen::Vector3d accum_error_;

  Eigen::Matrix3d Kd_lin_mat;
  Eigen::Matrix3d Kp_lin_mat;
  Eigen::Matrix3d Ki_lin_mat;
  Eigen::Matrix3d Kp_ang_mat;

  Eigen::Matrix3d Rot_matrix;
  float antiwindup_cte_ = 1.0f;

  float u1 = 0.0;
  float u2[3] = {0.0, 0.0, 0.0};

  std::array<std::array<float, 3>, 4> refs_;

  public:
  PD_controller();
  ~PD_controller(){};

  // void updateErrors();
  void computeActions();
  void publishActions();

  void setup();
  void run();

  std::unordered_map<std::string,double> parameters_;
  void update_gains(const std::unordered_map<std::string,double>& params){
    // for (auto it = params.begin(); it != params.end(); it++) {
    //   RCLCPP_INFO(this->get_logger(), "Updating gains: %s = %f", it->first.c_str(), it->second);
    // }
#if SPEED_REFERENCE == 1
    Eigen::Vector3d Kp_lin(params.at("speed_following.speed_Kp.x"),
                           params.at("speed_following.speed_Kp.y"),
                           params.at("speed_following.speed_Kp.z"));
    Eigen::Vector3d Kd_lin(params.at("speed_following.speed_Kd.x"),
                            params.at("speed_following.speed_Kd.y"),
                            params.at("speed_following.speed_Kd.z"));
    Eigen::Vector3d Ki_lin(params.at("speed_following.speed_Ki.x"),
                            params.at("speed_following.speed_Ki.y"),
                            params.at("speed_following.speed_Ki.z"));
#else
    Eigen::Vector3d Kp_lin(params.at("trajectory_following.position_Kp.x"),
                           params.at("trajectory_following.position_Kp.y"),
                           params.at("trajectory_following.position_Kp.z"));
    Eigen::Vector3d Kd_lin(params.at("trajectory_following.position_Kd.x"),
                           params.at("trajectory_following.position_Kd.y"),
                           params.at("trajectory_following.position_Kd.z"));
    Eigen::Vector3d Ki_lin(params.at("trajectory_following.position_Ki.x"),
                           params.at("trajectory_following.position_Ki.y"),
                           params.at("trajectory_following.position_Ki.z"));
#endif
    Eigen::Vector3d Kp_ang(params.at("angular_speed_controller.angular_gain.x"),
                           params.at("angular_speed_controller.angular_gain.y"),
                           params.at("angular_speed_controller.angular_gain.z"));
    Kp_lin_mat = Kp_lin.asDiagonal();
    Kd_lin_mat = Kd_lin.asDiagonal();
    Ki_lin_mat = Ki_lin.asDiagonal();
    Kp_ang_mat = Kp_ang.asDiagonal();

    mass = params.at("uav_mass");
    antiwindup_cte_ = params.at("antiwindup_cte");
  };

  void CallbackOdomTopic(const nav_msgs::msg::Odometry::SharedPtr odom_msg);

  rcl_interfaces::msg::SetParametersResult parametersCallback(
      const std::vector<rclcpp::Parameter> &parameters) {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;
    result.reason = "success";
    for (auto& param:parameters){
      // check if the parameter is defined in parameters_
      if (parameters_.find(param.get_name()) != parameters_.end()){
        parameters_[param.get_name()] = param.get_value<double>();
      }
      else {
        result.successful = false;
        result.reason = "parameter not found";
      }
    }
    if (result.successful){
      update_gains(parameters_);
    }
    return result;
  }

  private:
  Vector3d computeForceDesiredByTraj();
  Vector3d computeForceDesiredBySpeed();
  void CallbackTrajTopic(const trajectory_msgs::msg::JointTrajectoryPoint::SharedPtr traj_msg);
};

#endif
