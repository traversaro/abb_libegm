/***********************************************************************************************************************
 *
 * Copyright (c) 2015, ABB Schweiz AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that
 * the following conditions are met:
 *
 *    * Redistributions of source code must retain the
 *      above copyright notice, this list of conditions
 *      and the following disclaimer.
 *    * Redistributions in binary form must reproduce the
 *      above copyright notice, this list of conditions
 *      and the following disclaimer in the documentation
 *      and/or other materials provided with the
 *      distribution.
 *    * Neither the name of ABB nor the names of its
 *      contributors may be used to endorse or promote
 *      products derived from this software without
 *      specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***********************************************************************************************************************
 */

#ifndef EGM_TRAJECTORY_INTERFACE_H
#define EGM_TRAJECTORY_INTERFACE_H

#include <queue>

#include "egm_wrapper_trajectory.pb.h" // Generated by Google Protocol Buffer compiler protoc

#include "egm_base_interface.h"
#include "egm_common.h"
#include "egm_interpolator.h"

namespace abb
{
namespace egm
{
/**
 * \brief Class for an EGM trajectory user interface.
 *
 * The class provides behavior for following trajectories provided by an external user, and this includes:
 * - Processing asynchronous callbacks from an UDP server.
 * - Queuing ordered trajectories, and follow them in order.
 * - Providing methods for interacting with the trajectory execution (e.g. stop and resume execution).
 */
class EGMTrajectoryInterface : public EGMBaseInterface
{
public:
  /**
   * \brief A constructor.
   *
   * \param io_service for operating boost asio's asynchronous functions.
   * \param port_number for the server's UDP socket.
   * \param configuration for the interface's configuration.
   */
  EGMTrajectoryInterface(boost::asio::io_service& io_service,
                         const unsigned short port_number,
                         const TrajectoryConfiguration& configuration = TrajectoryConfiguration());

  /**
   * \brief Retrive the interface's current configuration.
   *
   * \return TrajectoryConfiguration containing the current configuration.
   */
  TrajectoryConfiguration getConfiguration();
  
  /**
   * \brief Update the interface's configuration (update is only applied for new EGM communication sessions).
   *
   * \param configuration containing the configuration update.
   */
  void setConfiguration(const TrajectoryConfiguration& configuration);

  /**
   * \brief Add a trajectory to the execution queue.
   *
   * \param trajectory containing the trajectory to add.
   * \param override_trajectories indicating if all pending trajectories should be overridden (i.e. removed).
   */
  void addTrajectory(const wrapper::trajectory::TrajectoryGoal trajectory, const bool override_trajectories = false);

  /**
   * \brief Stop the trajectory motion execution.
   *
   * Note: The intention is to only use this for short temporary stops, for long stops it is recommended to stop the
   *       EGM communication session completely. A resume normally needs to be ordered for execution to start again.
   *
   * \param discard_trajectories indicating if all pending trajectories should be discarded (i.e. removed).
   */
  void stop(const bool discard_trajectories = false);

  /**
   * \brief Resume the trajectory motion execution (after a stop has occurred).
   */
  void resume();

  /**
   * \brief Update the duration scaling factor for trajectory goals.
   * 
   * Note: Only values between 1.0 and 5.0 will be considered. E.g. if the factor is 2.0,
   *       then the remaining duration will be doubled. As will all upcoming goal durations.
   *
   * \param factor containing the new scale factor.
   */
  void updateDurationFactor(double factor);
  
  /**
   * \brief Start to follow a static goal.
   * 
   * Note: Any current trajectory motions will be stopped before starting to follow the static goal.
   *
   * \param discard_trajectories indicating if all pending trajectories should be discarded (i.e. removed).
   */
  void startStaticGoal(const bool discard_trajectories = false);
  
  /**
   * \brief Set a static position goal to follow.
   *
   * \param position_goal containing the static position goal to follow.
   * \param fast_transition indicating if a fast transition should be done. I.e. skip ramp out of current goal.
   */
  void setStaticGoal(const wrapper::trajectory::StaticPositionGoal& position_goal, const bool fast_transition = false);
  
  /**
   * \brief Set a static velocity goal to follow.
   *
   * \param velocity_goal containing the static velocity goal to follow.
   * \param fast_transition indicating if a fast transition should be done. I.e. skip ramp out of current goal.
   */
  void setStaticGoal(const wrapper::trajectory::StaticVelocityGoal& velocity_goal, const bool fast_transition = false);

  /**
   * \brief Finish following a static goal.
   *
   * \param resume indicating if normal trajectory motion execution should be resumed automatically.
   */
  void finishStaticGoal(const bool resume = false);
  
  /**
   * \brief Retrieve an execution progress from the trajectory interface.
   *
   * \param p_execution_progress for containing the execution progress.
   *
   * \return bool indicating if the execution progress has been successfully retrieved or not.
   */
  bool retrieveExecutionProgress(wrapper::trajectory::ExecutionProgress* p_execution_progress);

private:
  /**
   * \brief Struct for containing the configuration data.
   */
  struct ConfigurationContainer
  {
    /**
     * \brief A constructor.
     *
     * \param initial specifying the initial configuration.
     */
    ConfigurationContainer(const TrajectoryConfiguration& initial)
    :
    active(initial),
    update(initial),
    has_pending_update(false)
    {}

    /**
     * \brief The active configuration.
     */
    TrajectoryConfiguration active;

    /**
     * \brief The configuration update.
     */
    TrajectoryConfiguration update;

    /**
     * \brief Flag indicating if the active configuration should be updated.
     */
    bool has_pending_update;

    /**
     * \brief Mutex for protecting the configuration.
     */
    boost::mutex mutex;
  };

  /**
   * \brief Class for managing the points, in a trajectory, that the robot should pass through.
   */
  class Trajectory
  {
  public:
    /**
     * \brief Default constructor.
     */
    Trajectory() {}

    /**
     * \brief A constructor.
     *
     * param trajectory for a trajectory to parse.
     */
    Trajectory(const wrapper::trajectory::TrajectoryGoal& trajectory)
    {
      for (int i = 0; i < trajectory.points_size(); ++i)
      {
        addTrajectoryPointBack(trajectory.points().Get(i));
      }
    }

    /**
     * \brief Add a point to the front of the queue.
     *
     * \param point to store.
     */
    void addTrajectoryPointFront(const wrapper::trajectory::PointGoal& point)
    {
      points_.push_front(point);
    }

    /**
     * \brief Add a point to the back of the queue.
     *
     * \param point to store.
     */
    void addTrajectoryPointBack(const wrapper::trajectory::PointGoal& point)
    {
      points_.push_back(point);
    }

    /**
     * \brief Retrive a point from the queue.
     *
     * \param p_point for storing the retrived point.
     *
     * \return bool indicating if a point was retrived or not.
     */
    bool retriveNextTrajectoryPoint(wrapper::trajectory::PointGoal* p_point)
    {
      bool result = false;

      if (p_point && !points_.empty())
      {
        p_point->CopyFrom(points_.front());
        points_.pop_front();
        result = true;
      }

      return result;
    }

    /**
     * \brief Copy the whole queue to a trajectory container.
     *
     * \param p_trajectory for containing the queue.
     */
    void copyTo(wrapper::trajectory::TrajectoryGoal* p_trajectory)
    {
      std::deque<wrapper::trajectory::PointGoal>::const_iterator i;

      if (p_trajectory)
      {
        for (i = points_.begin(); i != points_.end(); ++i)
        {
          p_trajectory->add_points()->CopyFrom(*i);
        }
      }
    }

    /**
     * \brief Retrive the number of points in the queue.
     *
     * \return size_t indicating the number of points in the queue.
     */
    size_t size()
    {
      return points_.size();
    }

  private:
    /**
     * \brief Container for the points in the trajectory.
     */
    std::deque<wrapper::trajectory::PointGoal> points_;
  };

  /**
   * \brief Class for managing trajectory motion data, between an external user and the EGM communication loop.
   */
  class TrajectoryMotion
  {    
  public:
    /**
     * \brief A constructor.
     *
     * \param configurations specifying the interface's initial configurations.
     */
    TrajectoryMotion(const TrajectoryConfiguration& configurations)
    :
    DURATION_FACTOR_MIN(1.0),
    DURATION_FACTOR_MAX(5.0),
    configurations_(configurations),
    motion_step_(configurations)
    {}
    
    /**
     * \brief Update the interface's configurations.
     *
     * \param configurations specifying the interface's new configurations.
     */
    void updateConfigurations(const TrajectoryConfiguration& configurations)
    {
      configurations_ = configurations;
      motion_step_.updateConfigurations(configurations);
    }
        
    /**
     * \brief Generate outputs, based on the current goal and e.g. the use of spline interpolation.
     *
     * \param p_outputs for containing the outputs.
     * \param inputs containing the inputs from the robot controller.
     */
    void generateOutputs(wrapper::Output* p_outputs, const InputContainer& inputs);

    /**
     * \brief Add a trajectory to the execution queue.
     *
     * \param trajectory containing the trajectory to add.
     * \param override_trajectories indicating if all pending trajectories should be overridden (i.e. removed).
     */
    void addTrajectory(const wrapper::trajectory::TrajectoryGoal& trajectory, const bool override_trajectories);

    /**
     * \brief Stop the trajectory motion execution.
     *
     * Note: A resume normally needs to be ordered for execution to start again.
     *
     * \param discard_trajectories indicating if all pending trajectories should be discarded (i.e. removed).
     */
    void stop(const bool discard_trajectories);

    /**
     * \brief Resume the trajectory motion execution (after a stop has occurred).
     */
    void resume();
    
    /**
     * \brief Update the duration scaling factor for trajectory goals.
     * 
     * Note: Only values between 1.0 and 5.0 will be considered. E.g. if the factor is 2.0,
     *       then the remaining duration will be doubled. As will all upcoming goal durations.
     *
     * \param factor containing the new scale factor.
     */
    void updateDurationFactor(double factor);
  
    /**
     * \brief Start to follow a static goal.
     * 
     * Note: Any current trajectory motions will be stopped before starting to follow the static goal.
     *
     * \param discard_trajectories indicating if all pending trajectories should be discarded (i.e. removed).
     */
    void startStaticGoal(const bool discard_trajectories);

    /**
     * \brief Set a static position goal to follow.
     *
     * \param position_goal containing the static position goal to follow.
     * \param fast_transition indicating if a fast transition should be done. I.e. skip ramp out of current goal.
     */
    void setStaticGoal(const wrapper::trajectory::StaticPositionGoal& position_goal, const bool fast_transition);
  
    /**
     * \brief Set a static velocity goal to follow.
     *
     * \param velocity_goal containing the static velocity goal to follow.
     * \param fast_transition indicating if a fast transition should be done. I.e. skip ramp out of current goal.
     */
    void setStaticGoal(const wrapper::trajectory::StaticVelocityGoal& velocity_goal, const bool fast_transition);

    /**
     * \brief Finish following a static goal.
     *
     * \param resume indicating if normal trajectory motion execution should be resumed automatically.
     */
    void finishStaticGoal(const bool resume);
    
    /**
     * \brief Retrieve an execution progress from the trajectory interface.
     *
     * \param p_progress for containing the execution progress.
     *
     * \return bool indicating if the execution progress has been recently updated or not.
     */
    bool retrieveExecutionProgress(wrapper::trajectory::ExecutionProgress* p_progress);

  private:
    /**
     * \brief Enum for the different execution states the interface can handle.
     */
    enum States
    {
      Normal,    ///< \brief Retrieve goals from the current trajectory.
      RampDown,  ///< \brief Ramp down the current velocity references.
      StaticGoal ///< \brief Follow either static position or velocity goals.
    };
      
    /**
     * \brief Enum for the different execution sub states the interface can handle.
     */
    enum SubStates
    {
      None,    ///< \brief The current state has no active sub state.
      Running, ///< \brief The current state has a running sub state.
      Finished ///< \brief The current state has finished a sub state.
    };

    /**
     * \brief Container for decision data, used to decide what to do during execution of trajectory motions.
     */
    struct DecisionData
    {
      /**
       * \brief Container for pending events.
       */
      struct PendingEvents
      {
        /**
         * \brief Default constructor.
         */
        PendingEvents()
        :
        do_stop(false),
        do_resume(false),
        do_discard(false),
        do_ramp_down(false),
        do_static_goal_start(false),
        do_static_goal_fast_update(false),
        do_static_position_goal_update(false),
        do_static_velocity_goal_update(false),
        do_static_goal_finish(false),
        do_duration_factor_update(false),
        duration_factor(1.0)
        {};
        
        /**
         * \brief Flag indicating if the current velocities should be completely ramped out.
         */
        bool do_stop;
        
        /**
         * \brief Flag indicating if the execution should resume after a complete stop has occurred.
         */
        bool do_resume;

        /**
         * \brief Flag indicating if the current trajectories should be discarded.
         */
        bool do_discard;

        /**
         * \brief Flag indicating if the current motion's velocity should be ramped down.
         */
        bool do_ramp_down;
      
        /**
         * \brief Flag indicating if static goal execution should be started.
         */
        bool do_static_goal_start;
        
        /**
         * \brief Flag indicating if the static goal should be updated fast.
         */
        bool do_static_goal_fast_update;

        /**
         * \brief Flag indicating if the static position goal should be updated.
         */
        bool do_static_position_goal_update;
        
        /**
         * \brief Flag indicating if the static velocity goal should be updated.
         */
        bool do_static_velocity_goal_update;

        /**
         * \brief Flag indicating if the static goal execution should be finished.
         */
        bool do_static_goal_finish;
        
        /**
         * \brief The pending static position goal to follow.
         */
        wrapper::trajectory::StaticPositionGoal static_position_goal;

        /**
        * \brief The pending static velocity goal to follow.
        */
        wrapper::trajectory::StaticVelocityGoal static_velocity_goal;

        /**
         * \brief Flag indicating if the duration scale factor should be updated.
         */
        bool do_duration_factor_update;

        /**
         * \brief The pending duration scale factor update.
         */
        double duration_factor;
      };

      /**
       * \brief Default constructor.
       */
      DecisionData()
      :
      has_new_goal(false),
      has_active_goal(false),
      state(Normal),
      sub_state(None),
      has_updated_execution_progress(false)
      {}

      /**
       * \brief Flag indicating if there is a new goal.
       *
       * Note: A new goal implies that the interpolator should be updated (e.g. calculation of coefficients).
       */
      bool has_new_goal;

      /**
       * \brief Flag indicating if there is an active goal.
       */
      bool has_active_goal;

      /**
       * \brief The current state.
       */
      States state;

      /**
       * \brief The current sub state.
       */
      SubStates sub_state;

      /**
       * \brief The pending events for the trajectory motion execution.
       */
      PendingEvents pending_events;
      
      /**
       * \brief Flag indicating if the execution progress has been updated or not.
       */
      bool has_updated_execution_progress;

      /**
       * \brief The interface's execution progress.
       */
      wrapper::trajectory::ExecutionProgress execution_progress;

      /**
       * \brief Mutex for protecting the data.
       */
      boost::mutex mutex;
    };
    
    /**
     * \brief Container for trajectory data. E.g. trajectory queues, and the currently active trajectory. 
     */
    struct TrajectoryContainer
    {
      /**
       * \brief Queue for storing trajectories to execute.
       */
      std::deque<boost::shared_ptr<Trajectory> > primary_queue;

      /**
       * \brief Queue for temporary storing trajectories to execute (e.g. during a discard trajectories event).
       */
      std::deque<boost::shared_ptr<Trajectory> > temporary_queue;
    
      /**
       * \brief The currently active trajectory.
       */
      boost::shared_ptr<Trajectory> p_current;
      
      /**
       * \brief Mutex for protecting the data.
       */
      boost::mutex mutex;
    };

    /**
     * \brief Class for managing motion step data. 
     */
    class MotionStep
    {
    public:
      /**
       * \brief Container for process data, used for processing motion steps.
       */
      struct ProcessData
      {
        /**
         * \brief Default constructor.
         */
        ProcessData()
        :
        mode(EGMJoint),
        time_passed(0.0),
        estimated_sample_time(Constants::RobotController::LOWEST_SAMPLE_TIME),
        duration_factor(1.0)
        {}

        /**
         * \brief The assumed active EGM mode.
         */
        EGMModes mode;

        /**
         * \brief The time passed for the current goal execution.
         */
        double time_passed;

        /**
         * \brief The estimated sample time.
         */
        double estimated_sample_time;
      
        /**
         * \brief A scaling factor for the goal duration.
         */
        double duration_factor;
        
        /**
         * \brief Container for the current robot feedback values.
         */
        wrapper::Feedback feedback;   
      };
      
      /**
       * \brief A constructor.
       *
       * \param configurations specifying the trajectory interface's initial configurations.
       */
      MotionStep(const TrajectoryConfiguration& configurations)
      :
      CONDITION(0.005),
      RAMP_DOWN_STOP_DURATION(1.0),
      STATIC_GOAL_DURATION(5.0),
      STATIC_GOAL_DURATION_SHORT(0.1),
      condition_met_(true),
      configurations_(configurations)
      {}

      /**
       * \brief Update the interface's configurations.
       *
       * \param configurations specifying the interface's new configurations.
       */
      void updateConfigurations(const TrajectoryConfiguration& configurations)
      {
        configurations_ = configurations;
      }

      /**
       * \brief Reset the motion step data.
       */
      void resetMotionStep();
    
      /**
       * \brief Prepare for a normal goal.
       *
       * \param last_point indicating if it is the last point in the current trajectory.
       */
      void prepareNormalGoal(const bool last_point);
      
      /**
       * \brief Prepare for a ramp down goal.
       *
       * \param do_stop indicating if a stop should be performed.
       */
      void prepareRampDownGoal(const bool do_stop);
      
      /**
       * \brief Prepare for a static position goal.
       *
       * \param position_goal containing the static goal to follow.
       * \param fast_transition indicating if a fast transition should be done.
       */
      void prepareStaticGoal(const wrapper::trajectory::StaticPositionGoal& position_goal, const bool fast_transition);
      
      /**
       * \brief Prepare for a static velocity goal.
       *
       * \param velocity_goal containing the static goal to follow.
       * \param fast_transition indicating if a fast transition should be done.
       */
      void prepareStaticGoal(const wrapper::trajectory::StaticVelocityGoal& velocity_goal, const bool fast_transition);

      /**
       * \brief Check if the goal conditions has been met.
       *
       * \return bool indicating if the goal condition has been met or not.
       */
      bool conditionMet();

      /**
       * \brief Check if the interpolation duration has been reached.
       *
       * \return bool indicating if the interpolation duration has been reached or not.
       */
      bool interpolationDurationReached()
      {
        return ((interpolator.getDuration() - data.time_passed) < 0.5*Constants::RobotController::LOWEST_SAMPLE_TIME);
      }
      
      /**
       * \brief Update the interpolator according to the specified internal goal.
       *
       * E.g. used after a new point has been activated in a trajectory.
       */
      void updateInterpolator()
      {
        data.time_passed = 0.0;
        interpolation.set_reach(internal_goal.reach());
        interpolation.set_duration(interpolator_conditions_.duration);
        interpolator.update(interpolation, internal_goal, interpolator_conditions_);
      }

      /**
       * \brief Evaluate the interpolator (at the next time instance).
       */
      void evaluateInterpolator()
      {
        data.time_passed += data.estimated_sample_time;
        interpolator.evaluate(&interpolation, data.estimated_sample_time, data.time_passed);
      }

      /**
       * \brief Data used during the processing of motion steps.
       */
      ProcessData data;

      /**
       * \brief The internal goal point (updated with the data present in the external goal point).
       */
      wrapper::trajectory::PointGoal internal_goal;

      /**
       * \brief The external goal point (retrieved from external user input).
       */
      wrapper::trajectory::PointGoal external_goal;

      /**
       * \brief The interpolation (i.e. reference point to the robot controller).
       */
      wrapper::trajectory::PointGoal interpolation;
      
      /**
       * \brief The interpolation manager.
       */
      EGMInterpolator interpolator;

    private:
      /**
       * \brief Estimate the duration for the internal goal.
       *
       * Note: Should only be used if no duration has been specified externally.
       *
       * \return double containing the estimation.
       */
      double estimateDuration();
      
      /**
       * \brief Check if the conditions has been satisfied for a joint goal.
       *
       * \param goal containing the goal.
       * \param feedback containing the feedback.
       */
      void checkConditions(const wrapper::Joints& goal, const wrapper::Joints& feedback);
      
      /**
       * \brief Check if the conditions has been satisfied for a Cartesian goal.
       *
       * \param goal containing the goal.
       * \param feedback containing the feedback.
       */
      void checkConditions(const wrapper::Cartesian& goal, const wrapper::Cartesian& feedback);
      
      /**
       * \brief Check if the conditions has been satisfied for a quaternion goal.
       *
       * \param goal containing the goal.
       * \param feedback containing the feedback.
       */
      void checkConditions(const wrapper::Quaternion& goal, const wrapper::Quaternion& feedback);
      
      /**
       * \brief Transfer values from an external robot goal to the internal goal.
       *
       * \param source containing the values to transfer.
       */
      void transfer(const wrapper::trajectory::RobotGoal& source);
      
      /**
       * \brief Transfer values from an external external goal to the internal goal.
       *
       * \param source containing the values to transfer.
       */
      void transfer(const wrapper::trajectory::ExternalGoal& source);
      
      /**
       * \brief Transfer values from an external static position goal to the internal goal.
       *
       * \param source containing the values to transfer.
       */
      void transfer(const wrapper::trajectory::StaticPositionGoal& source);
      
      /**
       * \brief Transfer values from an external static velocity goal to the internal goal.
       *
       * \param source containing the values to transfer.
       */
      void transfer(const wrapper::trajectory::StaticVelocityGoal& source);
      
      /**
       * \brief Constant for a condition [degrees or mm] for when a point is considered to be reached.
       */
      const double CONDITION;

      /**
       * \brief Constant for ramp down stop duration [s].
       */
      const double RAMP_DOWN_STOP_DURATION;

      /**
       * \brief Constant for static goal ramp in duration [s].
       */
      const double STATIC_GOAL_DURATION;
      
      /**
       * \brief Constant for static goal ramp in short duration [s].
       */
      const double STATIC_GOAL_DURATION_SHORT;

      /**
       * \brief Conditions for the interpolator.
       */
      EGMInterpolator::Conditions interpolator_conditions_;
      
      /**
       * \brief Flag indicating if the joint position and Cartesian pose conditions has been met.
       */
      bool condition_met_;

      /**
       * \brief The trajectory interface's configurations.
       */
      TrajectoryConfiguration configurations_;
    };

    /**
     * \brief Class for calculating outputs to send to the robot controller.
     *
     * Note: Includes ramp out of velocity, and acceleration, references for reducing risk of overshooting the target.
     *       This is applied for the normal execution state, and if linear interpolation or if the target is important
     *       to reach (e.g. last point in a trajectory).
     */
    class Controller
    {
    public:
      /**
       * \brief Default constructor.
       */
      Controller()
      :
      egm_mode_(EGMJoint),
      is_normal_state_(false),
      is_linear_(false),
      do_velocity_transition_(false),
      a_(1.0),
      b_(1.0),
      k_(1.0)
      {}
      
      /**
       * \brief Update the controller, and prepare for a new motion.
       *
       * \param state indicating the current state of the interface.
       * \param motion_step containing the assumed active EGM mode and initial reference values.
       * \param configurations containing the current configurations (e.g. active interpolation spline method).
       */
      void update(const States state,
                  const MotionStep& motion_step,
                  const TrajectoryConfiguration& configurations);

      /**
       * \brief Calculate the outputs to the robot controller.
       *
       * \param p_outputs for containing the output values.
       * \param p_motion_step containing the current references and feedback values.
       */
      void calculate(wrapper::Output* p_outputs, MotionStep* p_motion_step);

    private:
      /**
       * \brief Check if the velocity values should be transitioned. Also ramps out acceleration values if necessary.
       *
       * \param p_references for containing the current reference values.
       */
      void checkForVelocityTransition(wrapper::trajectory::PointGoal* p_references);
      
      /**
       * \brief Calculate the joint output (i.e. positions or velocities).
       *
       * \param p_out for containing the output values.
       * \param p_ref for the current reference values.
       * \param fdb for the current feedback values.
       * \param start for the start values.
       */
      void calculate(wrapper::Joints* p_out,
                     wrapper::Joints* p_ref,
                     const wrapper::Joints& fdb,
                     const wrapper::Joints& start);
      
      /**
       * \brief Calculate the Cartesian output (i.e. positions or linear velocities).
       *
       * \param p_out for containing the output values.
       * \param p_ref for the current reference values.
       * \param fdb for the current feedback values.
       * \param start for the start values.
       */
      void calculate(wrapper::Cartesian* p_out,
                     wrapper::Cartesian* p_ref,
                     const wrapper::Cartesian& fdb,
                     const wrapper::Cartesian& start);
      
      /**
       * \brief Calculate the Euler output (i.e. angular velocities).
       *
       * \param p_out for containing the output values.
       * \param p_ref for the current reference values.
       * \param fdb for the current feedback values.
       * \param start for the start values.
       */
      void calculate(wrapper::Euler* p_out,
                     wrapper::Euler* p_ref,
                     const wrapper::Euler& fdb,
                     const wrapper::Euler& start);

      /**
       * \brief Calculate the quaternion output.
       *
       * \param p_out for containing the output values.
       * \param ref for the current reference values.
       * \param fdb for the current feedback values.
       */
      void calculate(wrapper::Quaternion* p_out,
                     const wrapper::Quaternion& ref,
                     const wrapper::Quaternion& fdb);
      
      /**
       * \brief The assumed active EGM mode.
       */
      EGMModes egm_mode_;
      
      /**
       * \brief The initial references sent to the robot controller (for the current motion).
       */
      wrapper::trajectory::PointGoal initial_references_;
      
      /**
       * \brief Flag indicating if the interface is in normal state of not.
       */
      bool is_normal_state_;
      
      /**
       * \brief Flag indicating if linear interpolation is used or not.
       */
      bool is_linear_;

      /**
       * \brief Flag indicating that a velocity transition should be performed. 
       */
      bool do_velocity_transition_;

      /**
       * \brief Ramp factor that goes from 1 -> 0 according to 0.5*cos(pi*x) + 0.5, where x = [0, 1]. 
       */
      double a_;

      /**
       * \brief Ramp factor that goes from 1 -> 0 according to 0.5*cos(pi*x + pi) + 0.5, where x = [0, 1].
       */
      double b_;
      
      /**
       * \brief Proportional controller gain.
       */
      double k_;
    };
    
    /**
     * \brief Prepare the trajectory motion for the new callback.
     *
     * \param inputs containing the inputs from the robot controller.
     */
    void prepare(const InputContainer& inputs);

    /**
     * \brief Reset the trajectory motion data.
     */
    void resetTrajectoryMotion();

    /**
     * \brief Prepare the decision data.
     */
    void prepareDecisionData();
    
    /**
     * \brief Process the normal state.
     */
    void processNormalState();

    /**
     * \brief Process the ramp down state.
     */
    void processRampDownState();
    
    /**
     * \brief Process the static goal state.
     */
    void processStaticGoalState();

    /**
     * \brief Update the current goal, i.e. retrive a new goal point from the currently active trajectory.
     */
    void updateNormalGoal();
    
    /**
     * \brief Store the current goal, in the front of the currently active trajectory.
     */
    void storeNormalGoal();

    /**
     * \brief Maps the interface's current internal state to an execution progress state.
     *
     * The interface can be in any of the following states:
     * - Undefined state (should not occur).
     * - Normal state (references are generated from trajectories specified by a user).
     * - Ramp down state (ramping down any current references).
     * - Static goal state (references are generated from a single goal point specified by a user).
     *
     * \return ExecutionProgress_State with the execution progress state.
     */
    wrapper::trajectory::ExecutionProgress_State mapCurrentState();

    /**
     * \brief Constant for the minimum duration scale factor.
     */
    const double DURATION_FACTOR_MIN;

    /**
     * \brief Constant for the maximum duration scale factor.
     */
    const double DURATION_FACTOR_MAX;

    /**
     * \brief Data for making decisions during the execution of trajectory motions.
     */
    DecisionData data_;

    /**
     * \brief Manager for the motion steps, i.e. handle current goal and generating interpolation output.
     */
    MotionStep motion_step_;
    
    /**
     * \brief Controller for calculating the outputs to send to the robot controller, based on the interpolation results
     *        and current feedback.
     */
    Controller controller_;

    /**
     * \brief Container for the desired trajectories to follow, and the currently active trajectory.
     */
    TrajectoryContainer trajectories_;

    /**
     * \brief The trajectory interface's configurations.
     */
    TrajectoryConfiguration configurations_;
  };
  
  /**
   * \brief Initialize the callback.
   *
   * \param server_data containing the UDP server's callback data.
   *
   * \return bool indicating if the initialization succeeded or not.
   */
  bool initializeCallback(const UDPServerData& server_data);

  /**
   * \brief Handle callback requests from an UDP server.
   *
   * \param server_data containing the UDP server's callback data.
   *
   * \return string& containing the reply.
   */
  const std::string& callback(const UDPServerData& server_data);
  
  /**
   * \brief The interface's configuration.
   */
  ConfigurationContainer configuration_;

  /**
   * \brief The interface's trajectory motion data.
   */
  TrajectoryMotion trajectory_motion_;
};

} // end namespace egm
} // end namespace abb

#endif // EGM_TRAJECTORY_INTERFACE_H
