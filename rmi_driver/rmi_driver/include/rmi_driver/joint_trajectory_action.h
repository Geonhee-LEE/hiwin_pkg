/*
 * Copyright (c) 2017, Doug Smith
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *  Created on: Nov 29, 2017
 *      Author: Doug Smith
 */

/*
 * Sections of this code are either inspired by or taken directly from the robot_movement_interface and
 * industrial_robot_client.
 */

#ifndef INCLUDE_RMI_DRIVER_JOINT_TRAJECTORY_ACTION_H_
#define INCLUDE_RMI_DRIVER_JOINT_TRAJECTORY_ACTION_H_

#include <ros/ros.h>

#include "rmi_driver/commands.h"
#include "rmi_driver/rmi_logger.h"

#include <actionlib/server/action_server.h>

#include <control_msgs/FollowJointTrajectoryAction.h>
#include <control_msgs/FollowJointTrajectoryFeedback.h>
#include <robot_movement_interface/CommandList.h>
#include <robot_movement_interface/Result.h>
#include <trajectory_msgs/JointTrajectory.h>

namespace rmi_driver
{
typedef actionlib::ActionServer<control_msgs::FollowJointTrajectoryAction> JointTractoryActionServer;

class JointTrajectoryAction
{
public:
  JointTrajectoryAction(std::string ns, const std::vector<std::string> &joint_names, JtaCommandHandler *jta_handler);

  void newGoal(JointTractoryActionServer::GoalHandle &gh);

  /**
    * \brief Action server goal callback method
    *
    * \param gh goal handle
    *
    */
  void goalCB(JointTractoryActionServer::GoalHandle gh);

  /**
   * \brief Action server cancel callback method.
   *
   * This method will call abortGoal if there is an active command.
   *
   * \param gh goal handle
   *
   */
  void cancelCB(JointTractoryActionServer::GoalHandle gh);

  void subCB_CommandResult(const robot_movement_interface::ResultConstPtr &msg);

  /**
   * \brief Abort an active goal and send ABORT to the robot
   *
   * This method will send an ABORT command to the robot (always) and call setCanceled on the active goal (if active).
   *
   * @param error_code The error code to use in the result.
   * @param error_msg The error message to use in the result.
   */
  void abortGoal(int32_t error_code, const std::string &error_msg);

  void abortGoal();

  void reject(int32_t error_code, const std::string &error_msg);

  bool goalIsBusy(JointTractoryActionServer::GoalHandle &gh);

protected:
  /// Must be created before the action server
  ros::NodeHandle nh_;
  /**
   * \brief Internal action server
   */
  JointTractoryActionServer action_server_;

  std::string ns_;

  ros::Publisher pub_rmi_;

  ros::Subscriber sub_rmi_;

  JointTractoryActionServer::GoalHandle active_goal_;

  bool has_goal_;

  int last_cmd_id_;

  std::vector<std::string> conf_joint_names_;

  JtaCommandHandler *jta_handler_;

  rmi_log::RmiLogger logger_;
};
}  // namespace rmi_driver

#endif /* INCLUDE_RMI_DRIVER_JOINT_TRAJECTORY_ACTION_H_ */
