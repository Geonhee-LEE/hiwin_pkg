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

#include "rmi_driver/joint_trajectory_action.h"
#include "rmi_driver/util.h"

#include <vector>

namespace rmi_driver
{
JointTrajectoryAction::JointTrajectoryAction(std::string ns, const std::vector<std::string> &joint_names,
                                             JtaCommandHandler *jta_handler)
  : action_server_(nh_, ns + "/joint_trajectory_action", boost::bind(&JointTrajectoryAction::goalCB, this, _1),
                   boost::bind(&JointTrajectoryAction::cancelCB, this, _1), false)
  , conf_joint_names_(joint_names)
  , ns_(ns)
  , nh_(ns)
  , jta_handler_(jta_handler)
  , has_goal_(false)
  , logger_("JTA", ns)
{
  pub_rmi_ = nh_.advertise<robot_movement_interface::CommandList>("command_list", 2);

  sub_rmi_ = nh_.subscribe("command_result", 1, &JointTrajectoryAction::subCB_CommandResult, this);

  action_server_.start();

  logger_.INFO() << "joint_trajectory_handler started on topic " << ns + "/joint_trajectory_action";
}

void JointTrajectoryAction::newGoal(JointTractoryActionServer::GoalHandle &gh)
{
  // Will contain the full trajectory in the correct order.  It's much easier to rearrange everything now in 1 batch
  trajectory_msgs::JointTrajectory traj_sorted;

  auto &traj = gh.getGoal()->trajectory;

  auto &joint_names = traj.joint_names;

  if (joint_names.size() != conf_joint_names_.size())
  {
    reject(control_msgs::FollowJointTrajectoryResult::INVALID_JOINTS, "joint_names.size() != conf_joint_names_.size()");
    return;
  }

  // mapping[0] will contain the position of joint0 in the vectors.  This way I only have to search once.
  std::vector<size_t> mapping;

  for (auto &&name : conf_joint_names_)
  {
    auto idx = std::find(joint_names.begin(), joint_names.end(), name) - joint_names.begin();
    if (idx < joint_names.size())
      mapping.push_back(idx);
  }

  // Make sure the mapping is valid
  if (mapping.size() != joint_names.size())
  {
    reject(control_msgs::FollowJointTrajectoryResult::INVALID_JOINTS, "mapping.size() != joint_names.size()");
    return;
  }
  if (*std::max_element(mapping.begin(), mapping.end()) >= mapping.size())
  {
    reject(control_msgs::FollowJointTrajectoryResult::INVALID_JOINTS, "max element of mapping >= mapping size");
    return;
  }

  traj_sorted.header = traj.header;
  traj_sorted.joint_names = util::sortVectorByIndices<std::string>(mapping, traj.joint_names);

  last_cmd_id_ = 0;  // Reset the target cmd_id

  try
  {
    for (auto &&point : traj.points)
    {
      trajectory_msgs::JointTrajectoryPoint jtp;

      jtp.positions = util::sortVectorByIndices<double>(mapping, point.positions);
      jtp.velocities = util::sortVectorByIndices<double>(mapping, point.velocities);
      jtp.accelerations = util::sortVectorByIndices<double>(mapping, point.accelerations);
      jtp.effort = util::sortVectorByIndices<double>(mapping, point.effort);
      jtp.time_from_start = point.time_from_start;

      traj_sorted.points.push_back(jtp);
    }
  }
  catch (const std::runtime_error &error)
  {
    reject(control_msgs::FollowJointTrajectoryResult::INVALID_GOAL, error.what());
    return;
  }

  robot_movement_interface::CommandList cmd_list = jta_handler_->processJta(traj_sorted);

  if (cmd_list.commands.empty())
  {
    reject(control_msgs::FollowJointTrajectoryResult::INVALID_GOAL, "Unable to create a CommandList");
    return;
  }

  last_cmd_id_ = cmd_list.commands.back().command_id;  // Need to store it to know when the trajectory is complete

  gh.setAccepted();
  pub_rmi_.publish(cmd_list);
}

void JointTrajectoryAction::goalCB(JointTractoryActionServer::GoalHandle gh)
{
  logger_.INFO() << "goalCB new goal received";

  if (has_goal_)
  {
    logger_.INFO() << "A goal was active.  Aborting.";
    abortGoal();
  }
  // else
  //{
  active_goal_ = gh;
  has_goal_ = true;
  newGoal(gh);
  //}
}

void JointTrajectoryAction::subCB_CommandResult(const robot_movement_interface::ResultConstPtr &msg)
{
  if (has_goal_)
  {
    // If any message is an error, just stop
    if (msg->result_code != CommandResultCodes::OK && msg->result_code != CommandResultCodes::ABORT_OK)
    {
      abortGoal(-100, "subCB_CommandResult received a msg with a not-ok result code");
    }
    else if (msg->command_id == last_cmd_id_)
    {
      active_goal_.setSucceeded();
      has_goal_ = false;
    }
  }
}

void JointTrajectoryAction::cancelCB(JointTractoryActionServer::GoalHandle gh)
{
  // ROS_INFO_STREAM("CancelCB");

  logger_.INFO() << "JointTrajectoryAction::cancelCB called";
  // if (active_goal_.isValid() && active_goal_ == gh && goalIsBusy(active_goal_))
  if (has_goal_ && active_goal_ == gh)
  {
    abortGoal();
  }
}

bool JointTrajectoryAction::goalIsBusy(JointTractoryActionServer::GoalHandle &gh)
{
  if (!gh.isValid())
    return false;

  auto status = gh.getGoalStatus().status;

  return (status == actionlib_msgs::GoalStatus::PENDING || status == actionlib_msgs::GoalStatus::RECALLING ||
          status == actionlib_msgs::GoalStatus::ACTIVE || status == actionlib_msgs::GoalStatus::PREEMPTING);
}

void JointTrajectoryAction::abortGoal()
{
  abortGoal(-100, "Goal cancelled");
}

void JointTrajectoryAction::abortGoal(int32_t error_code, const std::string &error_msg)
{
  robot_movement_interface::CommandList cmd_list;
  robot_movement_interface::Command cmd;
  cmd.command_type = "ABORT";
  cmd_list.commands.push_back(cmd);
  cmd_list.replace_previous_commands = true;

  pub_rmi_.publish(cmd_list);
  ros::Duration(0.2).sleep();  // Give it a chance to actually abort.  @todo rethink this

  if (has_goal_)
  {
    control_msgs::FollowJointTrajectoryResult rslt;
    rslt.error_code = error_code;
    rslt.error_string = error_msg;

    active_goal_.setCanceled(rslt, error_msg);
    has_goal_ = false;
  }

  logger_.INFO() << "JointTrajectoryAction::abort: " << error_msg;
}

void JointTrajectoryAction::reject(int32_t error_code, const std::string &error_msg)
{
  if (has_goal_)
  {
    control_msgs::FollowJointTrajectoryResult rslt;
    rslt.error_code = error_code;
    rslt.error_string = error_msg;

    active_goal_.setRejected(rslt, error_msg);
    has_goal_ = false;
  }

  logger_.ERROR() << "JTA Rejected : " << error_msg;
}

}  // namespace rmi_driver
