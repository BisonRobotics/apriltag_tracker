#ifndef PROJECT_TAG_H
#define PROJECT_TAG_H

#include <list>

#include <tf2/transform_datatypes.h>
#include <tf2/LinearMath/Transform.h>
#include <boost/thread/thread.hpp>

#include <transform.h>
#include <errors.h>

namespace AprilTagTracker
{

class Tag
{
public:
  // TODO some of this should be made private
  Tag(int id, int priority, double size);

  // Thread safe
  void addTransform(TagDetection detection, tf2::Stamped<tf2::Transform> tag_tf,
                    tf2::Stamped<tf2::Transform> servo_tf, unsigned int seq);
  void setMapToTagTf(tf2::Transform tf);
  int getID();
  //std::string getFrameID(); Not thread safe
  cv::Point getDetectionCenter();
  unsigned int getSeq();
  double getGoodness();
  double getPriority();
  double getSize();
  std::vector<Transform> getTransforms();
  Transform getMovingAverageTransform();
  Transform getMovingAverageTransform(int number_of_transforms);
  Transform getMedianMovingAverageTransform();
  Transform getMedianFilteredTransform();
  Transform getMostRecentTransform();
  tf2::Transform getMapToTagTf();
  double getAngleFromCenter(int number_of_transforms);
  double getAngleFromCenter();

private:
  boost::mutex *mutex;
  int id;
  CompareType compare_mode;
  unsigned int seq;
  double goodness;
  double priority;
  double size;
  //std::string frame_id;

  int list_size;
  tf2::Transform map_to_tag_tf;
  std::list<AprilTagTracker::Transform> transforms;
};

}

#endif //PROJECT_TAG_H
