#include <apriltag_tracker.h>

namespace AprilTagTracker
{
  AprilTagTracker::AprilTagTracker()
  {
    tag_params.newQuadAlgorithm = true;
    tag_params.adaptiveThresholdValue = 12; //TODO figure out what this means and parameterize
    tag_family = new TagFamily("Tag36h11");
    tag_detector = new TagDetector(*tag_family, tag_params);
    tag_size = .0505;

    servo = new HostCommLayer::Dynamixel(0x11);

    // TODO parametrize and load from camera calibration
    camera.setWidth(640);
    camera.setHeight(480);
    camera.setFormat(raspicam::RASPICAM_FORMAT_GRAY);
    camera.setRotation(180);
    camera.open();
    while(!camera.isOpened()) // TODO add failure condition
    {
      std::cout << "Unable to open camera, waiting 0.5 seconds and trying again..." << std::endl;
      usleep(500000);
      camera.open();
    }
    camera_properties.fx = 730;
    camera_properties.fy = 730;
    camera_properties.px = 320;
    camera_properties.py = 240;
    camera_properties.fov.x = 62.2; // TODO measure
    camera_properties.fov.y = 48.8; // TODO measure
    camera_properties.width = camera.getWidth();
    camera_properties.height = camera.getHeight();
    camera_properties.optical_center.x = camera_properties.width / 2;
    camera_properties.optical_center.y = camera_properties.height / 2;
    camera_properties.degrees_per_pixel.x = camera_properties.fov.x / camera_properties.width;
    camera_properties.degrees_per_pixel.y = camera_properties.fov.y / camera_properties.height;

    // TODO what do the scalars do?
    image_gs = new cv::Mat(camera.getHeight(), camera.getWidth(), CV_8UC1, cv::Scalar(69,42,200));

    float degrees_per_pixel = camera_properties.fov.x / camera.getWidth();
    float dyn_resolution = 0.29;
  }

  AprilTagTracker::~AprilTagTracker()
  {
    delete image_gs;
    delete servo;
    delete tag_family;
    delete tag_detector;
  }

  Eigen::Matrix4f AprilTagTracker::getRelativeTransform(const cv::Point2f tagPts[])
  {
    std::vector<cv::Point3f> objPts;
    std::vector<cv::Point2f> imgPts;
    float s = tag_size/2.0f;
    objPts.emplace_back(cv::Point3f(-s,-s, 0));
    objPts.emplace_back(cv::Point3f( s,-s, 0));
    objPts.emplace_back(cv::Point3f( s, s, 0));
    objPts.emplace_back(cv::Point3f(-s, s, 0));

    imgPts.push_back(tagPts[0]);
    imgPts.push_back(tagPts[1]);
    imgPts.push_back(tagPts[2]);
    imgPts.push_back(tagPts[3]);

    cv::Mat rvec, tvec;
    CameraProperties cfg = camera_properties;
    cv::Matx33f cameraMatrix(
        cfg.fx,      0, cfg.px,
             0, cfg.fy, cfg.py,
             0,      0,      1);
    cv::Vec4f distParam(0,0,0,0); // all 0?
    cv::solvePnP(objPts, imgPts, cameraMatrix, distParam, rvec, tvec);
    cv::Matx33f r;
    cv::Rodrigues(rvec, r);
    Eigen::Matrix3f wRo;
    wRo << r(0,0), r(0,1), r(0,2), r(1,0), r(1,1), r(1,2), r(2,0), r(2,1), r(2,2);

    Eigen::Matrix4f T;
    T.topLeftCorner(3,3) = wRo;
    T.col(3).head(3) << tvec.at<float>(0), tvec.at<float>(1), tvec.at<float>(2);
    T.row(3) << 0,0,0,1;

    return T;
  }

  void AprilTagTracker::drawDetections(cv::Mat *image)
  {
    cv::Point2f *p;
    cv::Point2f cxy;
    size_t id;
    for (int i = 0; i < tag_detections.size(); i++)
    {
      p = tag_detections[i].p;
      cxy = tag_detections[i].cxy;
      id = tag_detections[i].id;

      // plot outline
      cv::line(*image, p[0], p[1], cv::Scalar(255,0,0,0) );
      cv::line(*image, p[1], p[2], cv::Scalar(0,255,0,0) );
      cv::line(*image, p[2], p[3], cv::Scalar(0,0,255,0) );
      cv::line(*image, p[3], p[0], cv::Scalar(255,0,255,0) );

      // mark center
      cv::circle(*image, cv::Point2f(cxy.x, cxy.y), 8, cv::Scalar(0,0,255,0), 2);

      // print ID
      std::ostringstream strSt;
      strSt << "#" << id;
      cv::putText(*image, strSt.str(),
                  cv::Point2f(cxy.x + 10, cxy.y + 10),
                  cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0,0,255));
    }
  }

void AprilTagTracker::getAndProcessImage()
{
  // Grab image
  get_and_process_data_time_points[0] = std::chrono::system_clock::now();
  camera.grab();
  capture_time = ros::Time::now();

  get_and_process_data_time_points[1] = std::chrono::system_clock::now();
  camera.retrieve(image_gs->data);

  get_and_process_data_time_points[2] = std::chrono::system_clock::now();

  // Process image
  tag_detector->process(*image_gs, camera_properties.optical_center, tag_detections);
  get_and_process_data_time_points[3] = std::chrono::system_clock::now();

}

void AprilTagTracker::getAndProcessImage(cv::Mat *image)
{
  AprilTagTracker::getAndProcessImage();

  // Return result
  image = image_gs;
}

void AprilTagTracker::adjustServo()
{
  // TODO implement using multiple tags
  if (tag_detections.size() == 1)
  {
    float difference = tag_detections[0].cxy.x - camera_properties.width / 2;
    float rotation = camera_properties.degrees_per_pixel.x * difference / servo->resolution;
    uint16_t position;
    servo->getPosition(&position);
    std::cout << "Original position: " << position;
    servo->setPosition((uint16_t)((double)position - rotation));
    std::cout << ", corrected position: " << position - rotation << std::endl;
  }
}

void AprilTagTracker::populateAprilTagDetectionsArray(apriltag_tracker::AprilTagDetectionArray *tag_detection_array)
{
  for (int i = 0; i < tag_detections.size(); i++)
  {
    // Get transform
    /* Transform */
    Eigen::Matrix4f transform = getRelativeTransform(tag_detections[i].p);
    Eigen::Matrix3f rotation = transform.block(0, 0, 3, 3);
    Eigen::Quaternion<float> rotation_q = Eigen::Quaternion<float>(rotation);

    // Build tag
    geometry_msgs::PoseStamped tag_pose;
    tag_pose.pose.position.x = transform(0, 3);
    tag_pose.pose.position.y = transform(1, 3);
    tag_pose.pose.position.z = transform(2, 3);
    tag_pose.pose.orientation.x = rotation_q.x();
    tag_pose.pose.orientation.y = rotation_q.y();
    tag_pose.pose.orientation.z = rotation_q.z();
    tag_pose.pose.orientation.w = rotation_q.w();
    tag_pose.header.frame_id ="camera";
    tag_pose.header.stamp = capture_time;
    tag_pose.header.seq = 0; // TODO make into sequence

    apriltag_tracker::AprilTagDetection tag_detection;
    tag_detection.pose = tag_pose;
    tag_detection.id = (int)tag_detections[i].id;
    tag_detection.size = tag_size;
    tag_detection_array->detections.push_back(tag_detection);
  }
}

void AprilTagTracker::outputTimingInfo()
{

}


}


