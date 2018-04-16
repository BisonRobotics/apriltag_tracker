#ifndef POSE_ESTIMATE_FILTER_H
#define POSE_ESTIMATE_FILTER_H

#include <geometry_msgs/PoseStamped.h>
#include <boost/thread/thread.hpp>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <tf2/transform_datatypes.h>
#include <tf2/LinearMath/Transform.h>


namespace apriltag_tracker
{

class PoseEstimateFilter
{
public:

  PoseEstimateFilter(int list_size, double max_dt);
  void addPoseEstimate(geometry_msgs::PoseStamped &pose);
  geometry_msgs::PoseStamped getMovingAverageTransform();
  geometry_msgs::PoseStamped getMovingAverageTransform(ros::Time current_time);
  void flushOldPoses(std::list<geometry_msgs::PoseStamped> *poses, ros::Time current_time);

  static void getRPY(tf2::Quaternion q, double &roll, double &pitch, double &yaw);
  static double getTheta(geometry_msgs::Quaternion orientation);
  static geometry_msgs::Quaternion getAverageOrientation(std::list<geometry_msgs::PoseStamped> &poses);
  static geometry_msgs::Point getAveragePosition(std::list<geometry_msgs::PoseStamped> &poses);

private:
  boost::mutex *mutex;
  int list_size;
  unsigned int seq;
  std::list<geometry_msgs::PoseStamped> poses;
  ros::Duration max_dt;
};

}

#endif //POSE_ESTIMATE_FILTER_H
