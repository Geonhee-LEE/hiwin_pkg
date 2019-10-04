/*
 * Copyright (c) 2017, Doug Smith, KEBA Corp
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
 *  Created on: Aug 3, 2017
 *      Author: Doug Smith
 */

#ifndef INCLUDE_COMMANDS_H_
#define INCLUDE_COMMANDS_H_

#include <ros/ros.h>

#include <robot_movement_interface/CommandList.h>

#include <control_msgs/FollowJointTrajectoryAction.h>
//#include <rmi_driver/joint_trajectory_action.h>

#include <string>

namespace rmi_driver
{
struct CommandResultCodes
{
  enum
  {
    OK = 0,
    FAILED_TO_FIND_HANDLER = 1,
    SOCKET_FAILED_TO_CONNECT = 2,
    ABORT_FAIL = 9998,
    ABORT_OK = 9999,

  };
};

/**
 * \brief Commands that will be sent to the robot controller as strings.
 *
 * Commands contain a list of pairs of strings.  The pairs represent a command/parameters with optional values.  At
 * command string (with optional values) is required.  Additional parameter pairs can be added.  toString is used to
 * create the actual string to the robot.  << is overridden for easier stream console output.
 *
 * Default string format:
 * \code
 * <command>[ : <values>]; [<param>[ : <values>];]
 * \endcode
 *
 * Examples: \n
 * "ptp : 1 2 3 4 5 6;"  A ptp move \n
 * "ptp : 1 2 3 4 5 6; speed : 100;" A ptp move with the speed parameter specified \n
 * "setting; speed : 100; overlap : 100;" A setting command that will set both the speed and overlap
 */
class RobotCommand
{
public:
  //! Each entry is stored as a pair<string, string>
  using CommandEntry = std::pair<std::string, std::string>;

  //! The full command with all optional params is stored in a vector.  Entry [0] is the actual command.
  using FullCommand = std::vector<CommandEntry>;

  /// Choose which socket to send over.  Cmd will be added to the command queue.  Get will be sent immediately.
  enum CommandType
  {
    Get,  //!< Get Commands that should return almost immediately.  get joint position, get digital io, etc
    Cmd   //!< Cmd Commands that might take some time.  Moving, waiting on inputs, etc
  };

  RobotCommand(CommandType type = CommandType::Cmd) : type_(type)
  {
    makeCommand(type, "", "");
  }

  RobotCommand(CommandType type, const std::string& command, std::string params = "") : type_(type)
  {
    makeCommand(type, command, params);
  }

  RobotCommand(CommandType type, const std::string& command, const std::vector<float>& floatVec) : type_(type)
  {
    makeCommand(type, command, RobotCommand::paramsToString(floatVec));
  }

  RobotCommand(const RobotCommand& other)
  {
    this->command_id_ = other.command_id_;
    this->full_command_ = other.full_command_;
    this->type_ = other.type_;
  }

  RobotCommand(RobotCommand&& other)
    : full_command_(std::move(other.full_command_)), type_(other.type_), command_id_(other.command_id_)
  {
  }

  /**
   * \brief Sets up the command.
   *
   * Sets the first element of full_command_ to (command, params).  It can optionally remove all existing parameters.
   * @param type Cmd or Get
   * @param command command string
   * @param command_vals parameters for the command
   * @param erase_params erase the full_command_ before setting
   */
  void makeCommand(CommandType type, std::string command, std::string command_vals, bool erase_params = false);

  virtual ~RobotCommand()
  {
    // std::cout << "Destroying: " << this->toString() << std::endl;
  }

  /**
   * Sets the command and values without modifying the parameters or type
   *
   * @param command command string
   * @param command_vals parameters for the command
   */
  void setCommand(std::string command, std::string command_vals)
  {
    makeCommand(type_, command, command_vals);
  }

  /**
   * Add a parameter and values
   *
   * @param param
   * @param param_vals
   */
  void addParam(std::string param, std::string param_vals);

  /**
   * \brief Prepare the string to send to the robot.
   *
   * See RobotCommand for the default format.
   *
   * @todo rethink this now that option params can be entered.  Or just make it virtual and leave it for someone else to
   * think about!
   * @return The full, formatted string that will be send to the robot
   */
  virtual std::string toString(bool append_newline = true) const;

  /**
   * \brief Check the response of a command.  It will call processResponse()
   *
   * By default it just checks if the response starts with "error".
   *
   * @param response The string returned by the robot
   * @return True if the response is OK
   */
  virtual bool checkResponse(std::string& response) const;

  /**
   * \brief Modify the response as needed.
   *
   * This can be used to do things like convert degrees to radians, reorder rotations, etc.  Does nothing by defaults.
   * @param response [in,out]
   */
  virtual void processResponse(std::string& response) const
  {
  }

  /**
   * Converts a float vector into a string of values separated by spaces.  Removes trailing zeroes
   *
   * @param floatVec vector of floats
   * @param precision default 4
   * @return string of values separated by spaces
   */
  static std::string paramsToString(const std::vector<float>& floatVec, int precision = 4);

  /**
   * Used to have an inheritance based << override.
   *
   * @param o operator<<(std::ostream& o, T t)
   * @return ostream containing the results of toString without a newline
   */
  virtual std::ostream& dump(std::ostream& o) const
  {
    std::string str = toString(false);
    o << str;
    return o;
  }

  // Getters/setters

  CommandType getType() const;
  void setType(CommandType type);
  std::string getCommand() const;

  int getCommandId() const;
  void setCommandId(int commandId);

protected:
  FullCommand full_command_;

  /// Used in the /command_result response
  int command_id_ = 0;

  CommandType type_;
};

class RobotCommandStatus : public RobotCommand
{
protected:
  std::string last_joint_state;
  std::string last_joint_vel;
  std::string last_tcp_frame;

public:
  RobotCommandStatus(CommandType type = CommandType::Cmd) : RobotCommand(type)
  {
  }

  RobotCommandStatus(CommandType type, const std::string& command, std::string params = "") : RobotCommand(type)
  {
  }

  RobotCommandStatus(CommandType type, const std::string& command, const std::vector<float>& floatVec)
    : RobotCommand(type)
  {
  }

  virtual void updateData(std::string& response)
  {
  }

  const std::string& getLastJointState()
  {
    return last_joint_state;
  }

  const std::string& getLastJointVel()
  {
    return last_joint_vel;
  }

  const std::string& getLastTcpFrame()
  {
    return last_tcp_frame;
  }
};

/**
 * \brief << ostream operator for RobotCommands
 *
 * @param o ostream
 * @param cmd the RobotCommand
 * @return << stream operator magic
 */
inline std::ostream& operator<<(std::ostream& o, const RobotCommand& cmd)
{
  return cmd.dump(o);
}

class CommandHandler;
class CommandRegister;
using RobotCommandPtr = std::shared_ptr<RobotCommand>;

/**
 * \brief Handle robot_movement_interface::Command and create Commands that are ready to send to the robot.
 *
 * Used to prepare command and parameter strings.  Provide the constructor with a similar message.
 * Fill out any fields you want checked.  Vectors will be checked for length, strings for equality.
 * Fields that have a length of 0 will be ignored.
 *
 * Extend this class, set up the sample_command_ in the constructor, and override processMsg.
 *
 * You can also create a base CommandHandler with a sample message and a std::function/lambda to process it.
 */
class CommandHandler
{
public:
  using CommandHandlerFunc = std::function<RobotCommandPtr(const robot_movement_interface::Command&)>;

  CommandHandler() : handler_name_("Base CommandHandler")
  {
  }

  virtual ~CommandHandler()
  {
  }

  /**
   * \brief Construct a new base CommandHandler that will call a std::function.  Used to quickly create a new handler
   without having to create a new class.
   *
   * Example:\n
   * \code{.cpp}
   *   CommandHandler chtest(cmd, [](const robot_movement_interface::Command& cmd_msg)
       {
         return Command(Command::CommandType::Cmd, cmd_msg.command_type, cmd_msg.pose_type);
       });
   * \endcode
   *
   * @param sample_command The sample command to match
   * @param f a std::function/lambda that takes a robot_movement_interface::Command returns a telnet Command
   */
  CommandHandler(const robot_movement_interface::Command& sample_command, CommandHandlerFunc f);

  /**
   * \brief Finish initializing any sample parameters that were not set up in the constructor.
   *
   * \details This is automatically called by CommandRegister::addHandler.  setCommandRegister will have already been
   * called so you can use info from the CommandRegister.  This is useful if you need to add checks based on the number
   * of joints.
   */
  virtual void initialize()
  {
  }

  /**
   * \brief Create a new unique_ptr<CommandHandler> that holds an extended CommandHandler of type T
   * @return a new unique_ptr<CommandHandler> pointing to T
   */
  template <typename T>
  static std::unique_ptr<CommandHandler> createHandler()
  {
    return std::unique_ptr<CommandHandler>(new T);
  }

  /**
   * \brief Create a new unique_ptr<CommandHandler> that holds CommandHandler of subtype T.  Constructed with a lambda.
   * @param sample_command The sample command to store.
   * @param f The processMsg function
   * @return a new unique_ptr<CommandHandler> pointing to T
   */
  template <typename T = CommandHandler>
  static std::unique_ptr<CommandHandler> createHandler(const robot_movement_interface::Command& sample_command,
                                                       CommandHandler::CommandHandlerFunc f)
  {
    return std::unique_ptr<CommandHandler>(new T(sample_command, f));
  }

  /**
   * \brief Checks if the values specified in the sample_command match those in cmd_msg.
   * \details Strings are checked for equality.  Vectors are checked for length.
   * @todo == probably isn't the operator to use here.  It's not checking "equality" it's just checking if it's a match.
   *
   * @param cmd_msg The message received from the command_list topic
   * @return True if it's a match
   */
  bool operator==(const robot_movement_interface::Command& cmd_msg);

  /**
   * \brief Processes a robot_movement_interface::Command.  Override this method in extended classes.
   *
   * \details Create a new std::shared_ptr<Command>.  The base version will create a base shared_ptr then call
   * the stored function processMsg(cmd_msg, telnet_command)
   *
   * @param cmd_msg The message received from the command_list topic
   * @return a new CommandPtr.  nullptr if processing the message failed.
   */
  virtual RobotCommandPtr processMsg(const robot_movement_interface::Command& cmd_msg) const;

  void setProcFunc(CommandHandlerFunc& f)
  {
    process_func_ = f;
  }

  virtual const CommandRegister* getCommandRegister() const
  {
    return command_register_;
  }

  /**
   * \brief Get the stored name of the handler.  Used for debug output.
   * @return The handler name
   */
  virtual std::string getName() const
  {
    return handler_name_;
  }

  /**
   * \brief Get the sample robot_movement_interface::Command message.
   * @todo Change the name of Command!!!
   */
  const robot_movement_interface::Command& getSampleCommand() const;

  virtual std::ostream& dump(std::ostream& o) const;

  void setCommandRegister(CommandRegister* commandRegister = nullptr)
  {
    command_register_ = commandRegister;
  }

  friend std::ostream& operator<<(std::ostream& o, const CommandHandler& cmdh);

protected:
  /// Pointer to the CommandRegister that owns this CommandHandler
  CommandRegister* command_register_ = nullptr;

  /// The stored sample command
  robot_movement_interface::Command sample_command_;

  /// The name of the handler.  Used for debug output.
  std::string handler_name_;

  /// The function to call when constructed by a lambda.
  CommandHandlerFunc process_func_ = nullptr;
};

/**
 * \brief Processes trajectory_msgs::JointTrajectory messages and turns them into a
 * robot_movement_interface::CommandList
 *
 * \warning JtaCommandHandler expects the JointTrajectory message to be sorted.  The order of the joints must match the
 * configured order in the driver.
 *
 * Call processJta() to process an entire trajectory.
 */
class JtaCommandHandler : public CommandHandler
{
public:
  JtaCommandHandler()
  {
    handler_name_ = "JTA Command";
  }

  virtual ~JtaCommandHandler()
  {
  }

  /**
   * \brief Get the command_id number that should be used next, starting at 0.
   *
   * @param cmd_list the command list
   * @return the command id to use
   */
  uint32_t getNextCommandId(robot_movement_interface::CommandList& cmd_list);

  /**
   * \brief Create the full robot_movement_interface::CommandList based on the JointTrajectory.
   *
   * It will call processFirstJtaPoint() for the first point, processJtaPoint() for every remaining point except for the
   * last, then processLastJtaPoint() for the final point.  This allows you to create multiple commands for the
   * first/last
   * points, to set any required settings or waits.
   *
   * @param joint_trajectory The full JointTrajectory message, with all values sorted in the same order as in this
   * driver.
   * @return The assembled CommandList to send
   */
  virtual robot_movement_interface::CommandList processJta(const trajectory_msgs::JointTrajectory& joint_trajectory);

  /**
   * \brief Process a point and append a Command to cmd_list.
   *
   * The default implementation will create a Command with:
   * \code
   * cmd.command_id = getNextCommandId();
   * cmd.command_type = "PTP";
   * cmd.pose_type = "JOINTS";
   * cmd.pose = copy(point.positions);
   * \endcode
   *
   * If point.accelerations, velocities, or effort is set, it will add:
   * \code
   * cmd.<param_type> = "ROS";
   * cmd.<param> = copy(point.<param>);
   * \endcode
   *
   * @param point The point to process
   * @param cmd_list The CommandList that is being assembled.
   */
  virtual void processJtaPoint(const trajectory_msgs::JointTrajectoryPoint& point,
                               robot_movement_interface::CommandList& cmd_list);

  /**
   * \brief Called by processJta() to handle the first point.
   *
   * Calls processJtaPoint() by default.  You can use this to perform any desired settings (changing modes, setting
   * tools,
   * etc) before the robot actually moves.
   *
   * @param point The point to process
   * @param cmd_list The CommandList that is being assembled.
   */
  virtual void processFirstJtaPoint(const trajectory_msgs::JointTrajectoryPoint& point,
                                    robot_movement_interface::CommandList& cmd_list)
  {
    processJtaPoint(point, cmd_list);
  }

  /**
   * \brief Called by processJta() to handle the last point.
   *
   * Calls processJtaPoint() by default.  You can use this to perform any desired commands after the robot stops moving.
   * For example, waiting until the robot has stopped moving.
   *
   * @param point The point to process
   * @param cmd_list The CommandList that is being assembled.
   */
  virtual void processLastJtaPoint(const trajectory_msgs::JointTrajectoryPoint& point,
                                   robot_movement_interface::CommandList& cmd_list)
  {
    processJtaPoint(point, cmd_list);
  }

protected:
};

/**
 * \brief This class contains all the registered command handlers.
 *
 * \details You should override this class in your plugin.  Implement registerCommandHandlers and add all your custom
 * CommandHandlers there.
 */
class CommandRegister
{
public:
  using CommandHandlerPtrVec = std::vector<std::unique_ptr<CommandHandler>>;

  CommandRegister()
  {
    jta_command_handler_.reset(new JtaCommandHandler);
  }

  virtual ~CommandRegister()
  {
  }

  /**
   * @todo this should be used like the v2 industrial client's joint map to support multiple robots
   *
   * @param joints
   */
  virtual void initialize(const std::vector<std::string>& joints) = 0;

  /**
   * Get the version string.  This will be checked against the string returned by the robot
   * @return version string
   */
  virtual const std::string& getVersion() = 0;

  /**
   * \brief Add a CommandHandler.  This will std::move a handler into the vector.
   *
   * @param handler rvalue reference to a handler.
   */
  void addHandler(std::unique_ptr<CommandHandler>&& handler)
  {
    command_handlers_.push_back(std::move(handler));
    command_handlers_.back()->setCommandRegister(this);
    command_handlers_.back()->initialize();
  }

  /**
   * \brief Add a CommandHandler of type T.
   *
   * Example:
   * addHandler<RobotCommandPtp>();
   */
  template <typename T>
  void addHandler()
  {
    // emplace will handle all of the stuff to create a unique_ptr
    command_handlers_.emplace_back(new T);
    command_handlers_.back()->setCommandRegister(this);
    command_handlers_.back()->initialize();
  }

  /**
   *
   * @param handler
   */
  void addHandler(CommandHandler&& handler)
  {
    command_handlers_.emplace_back(new CommandHandler(handler));
    command_handlers_.back()->setCommandRegister(this);
    command_handlers_.back()->initialize();
  }

  /**
   * Get the registered handlers
   * @return a reference to the vector of handlers
   */
  const CommandHandlerPtrVec& handlers() const
  {
    return command_handlers_;
  }

  /**
   * Search through the registered command handlers to find one that matches this message
   *
   * @param msg_cmd robot_movement_interface::Command that was received
   * @return const CommandHandler* that matched the msg_cmd.  nullptr if no match found
   */
  const CommandHandler* findHandler(const robot_movement_interface::Command& msg_cmd);

  /**
   * \brief Get the configured command handler.
   *
   * It will be the base JtaCommandHandler by default.
   * @return A pointer to the command handler.
   */
  virtual JtaCommandHandler* getJtaCommandHandler()
  {
    return jta_command_handler_.get();
  }

  /**
   * \brief Set the joint_trajectory_action command handler to type T
   *
   * This will be used by rmi_driver::JointTrajectoryAction to create a CommandList.
   */
  template <typename T>
  void setJtaCommandHandler()
  {
    jta_command_handler_.reset(new T);
  }

protected:
  /**
   * \brief Use addHandler to create and add new CommandHandlers to this register.
   *
   *
   */
  virtual void registerCommandHandlers() = 0;

  /// Vector of all registered command handlers
  CommandHandlerPtrVec command_handlers_;

  std::unique_ptr<JtaCommandHandler> jta_command_handler_ = std::unique_ptr<JtaCommandHandler>(new JtaCommandHandler);
};

using CommandRegisterPtr = std::shared_ptr<CommandRegister>;

}  // namespace rmi_driver

#endif /* INCLUDE_COMMANDS_H_ */
