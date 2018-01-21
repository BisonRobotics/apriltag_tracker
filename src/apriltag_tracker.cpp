#include <apriltag_tracker.h>
#include <geometry_msgs/TransformStamped.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>


namespace AprilTagTracker
{
  AprilTagTracker::AprilTagTracker(apriltag_tracker::Camera *camera, HostCommLayer::Dynamixel *servo,
                                   std::vector<TagInfo> *tag_info, TransformsCache transforms)
  {
    // TODO use dynamic reconfigure
    tag_params.newQuadAlgorithm = true;
    //tag_params.adaptiveThresholdValue = 12; //TODO figure out what this means and parametrize
    //tag_params.adaptiveThresholdValue = 20; //TODO figure out what this means and parametrize
    tag_family = new TagFamily("Tag36h11");
    tag_detector = new TagDetector(*tag_family, tag_params);

    this->servo = servo;
    this->tag_info = tag_info;
    this->camera = camera;
    this->transforms = transforms;
  }

  AprilTagTracker::~AprilTagTracker()
  {
    delete tag_family;
    delete tag_detector;
  }

  Eigen::Matrix4d AprilTagTracker::getRelativeTransform(const cv::Point2d tagPts[], double tag_size)
  {
    std::vector<cv::Point3d> objPts;
    std::vector<cv::Point2d> imgPts;
    double s = tag_size / 2.0;
    objPts.emplace_back(cv::Point3d(-s,-s, 0));
    objPts.emplace_back(cv::Point3d( s,-s, 0));
    objPts.emplace_back(cv::Point3d( s, s, 0));
    objPts.emplace_back(cv::Point3d(-s, s, 0));

    imgPts.push_back(tagPts[0]);
    imgPts.push_back(tagPts[1]);
    imgPts.push_back(tagPts[2]);
    imgPts.push_back(tagPts[3]);

    cv::Mat rvec, tvec;
    // TODO make sure the properties are accurate
    cv::solvePnP(objPts, imgPts, camera->getK(), camera->getD(), rvec, tvec);
    cv::Matx33d r;
    cv::Rodrigues(rvec, r);
    Eigen::Matrix3d wRo;
    wRo << r(0,0), r(0,1), r(0,2), r(1,0), r(1,1), r(1,2), r(2,0), r(2,1), r(2,2);

    Eigen::Matrix4d T;
    T.topLeftCorner(3,3) = wRo;
    T.col(3).head(3) << tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2);
    T.row(3) << 0,0,0,1;

    return T;
  }

  void AprilTagTracker::drawDetections()
  {
    cv::Point2d *p;
    cv::Point2d cxy;
    size_t id;
    for (int i = 0; i < tag_detections.size(); i++)
    {
      p = tag_detections[i].p;
      cxy = tag_detections[i].cxy;
      id = tag_detections[i].id;

      // plot outline
      cv::Mat *image = camera->getImagePtr();
      cv::line(*image, p[0], p[1], cv::Scalar(255,0,0,0) );
      cv::line(*image, p[1], p[2], cv::Scalar(0,255,0,0) );
      cv::line(*image, p[2], p[3], cv::Scalar(0,0,255,0) );
      cv::line(*image, p[3], p[0], cv::Scalar(255,0,255,0) );

      // mark center
      cv::circle(*image, cv::Point2d(cxy.x, cxy.y), 8, cv::Scalar(0,0,255,0), 2);

      // print ID
      std::ostringstream strSt;
      strSt << "#" << id;
      cv::putText(*image, strSt.str(),
                  cv::Point2d(cxy.x + 10, cxy.y + 10),
                  cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0,0,255));
    }
  }

void AprilTagTracker::processImage()
{
  // TODO there is a full image copy here, see if we can pass by reference
  tag_detector->process(camera->getImage(), camera->getOpticalCenter(), tag_detections);
}

void AprilTagTracker::adjustServo()
{
  // TODO implement using multiple tags
  if (tag_detections.size() == 1)
  {
    // Adjust servo
    double difference = (tag_detections[0].cxy.x - camera->getWidth() / 2);
    double rotation = camera->getDegreesPerPixel().x * difference / servo->resolution;
    uint16_t position = servo->adjustPosition(-(int16_t) rotation);
  }
}

void AprilTagTracker::calculateTransforms(apriltag_tracker::AprilTagDetectionArray *tag_detection_array)
{
  for (int i = 0; i < tag_detections.size(); i++)
  {
    for (int j = 0; j < tag_info->size(); j++)
    {
      if (tag_detections[i].id == (*tag_info)[j].id)
      {
        // Get transform
        /* Transform */
        Eigen::Matrix4d transform = getRelativeTransform(tag_detections[i].p, (*tag_info)[j].size);
        Eigen::Matrix3d rotation = transform.block(0, 0, 3, 3);
        Eigen::Quaternion<double> rotation_q = Eigen::Quaternion<double>(rotation);

        // Build detection
        apriltag_tracker::AprilTagDetection tag_detection;
        tag_detection.id = (int)tag_detections[i].id;
        tag_detection.size = (*tag_info)[j].size;
        tag_detection.transform.header.stamp = camera->getCaptureTime();
        tag_detection.transform.header.seq = (*tag_info)[j].seq++;
        tag_detection.transform.header.frame_id = "camera_optical";
        tag_detection.transform.child_frame_id = std::string("tag") + std::to_string(tag_detections[i].id) + "_estimate";
        tag_detection.transform.transform.translation.x = transform(0, 3);
        tag_detection.transform.transform.translation.y = transform(1, 3);
        tag_detection.transform.transform.translation.z = transform(2, 3);
        tag_detection.transform.transform.rotation.x = rotation_q.x();
        tag_detection.transform.transform.rotation.y = rotation_q.y();
        tag_detection.transform.transform.rotation.z = rotation_q.z();
        tag_detection.transform.transform.rotation.w = rotation_q.w();

        tag_detection_array->detections.push_back(tag_detection);

        // Build transform
        tf2::Stamped<tf2::Transform> tag_transform;
        tf2::fromMsg(tag_detection.transform, tag_transform);
        (*tag_info)[j].mutex->lock();
        (*tag_info)[j].tag_transform = tag_transform;
        (*tag_info)[j].mutex->unlock();
      }
    }
  }
}

void AprilTagTracker::estimateRobotPose(geometry_msgs::TransformStamped *pose_estimate_msg)
{

  tf2::Transform pose_estimate = (*tag_info)[0].map_to_tag_tf * (*tag_info)[0].tag_transform.inverse()
                                 * transforms.camera_optical_to_servo_joint * servo->getTransform().inverse()
                                 * transforms.servo_base_link_to_base_link;
  tf2::Stamped<tf2::Transform> pose_estimate_stamped(pose_estimate, (*tag_info)[0].tag_transform.stamp_, "map");
  *pose_estimate_msg = tf2::toMsg(pose_estimate_stamped);
  pose_estimate_msg->child_frame_id = "apriltag_tracker_pose_estimate";
  pose_estimate_msg->transform.translation.z = 0.0; // Remove z component since we aren't using it.
}

void AprilTagTracker::outputTimingInfo()
{

}


}


