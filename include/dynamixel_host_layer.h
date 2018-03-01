#ifndef PROJECT_HOST_COMM_LAYER_H
#define PROJECT_HOST_COMM_LAYER_H

#include <cstdint>
#include <cmath>

#include <mraa.hpp>

#include <comm_layer_defs.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2/transform_datatypes.h>
#include <geometry_msgs/TransformStamped.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

namespace HostCommLayer
{
  class I2cInterface
  {
  public:
    virtual ~I2cInterface() {};
    virtual mraa::Result write(const uint8_t* data, int length) = 0;
    virtual int read(uint8_t* data, int length) = 0;
  };

  class MraaI2c : public I2cInterface
  {
  public:
    MraaI2c(int bus, uint8_t address);
    mraa::Result write(const uint8_t* data, int length);
    int read(uint8_t* data, int length);
  private:
    mraa::I2c *i2c;
    int bus;
    uint8_t address;
  };


  class Dynamixel
  {
  public:
    explicit Dynamixel(uint8_t i2c_address);
    explicit Dynamixel(I2cInterface *interface);

    void writeI2c(CLMessage32* message);
    void readI2c(CLMessage32* message);
    void setPosition(uint16_t position);
    void setVelocity(uint16_t velocity);
    void setPollingDt(uint16_t polling_dt);
    void getPosition(uint16_t *position);

    tf2::Transform getTransform();
    tf2::Stamped<tf2::Transform> getStampedTransform();
    geometry_msgs::TransformStamped getTransformMsg();
    void adjustCamera(int16_t velocity);
    void updatePosition();
    void scan();
    int16_t calculateDesiredVelocity(double theta);

    static uint8_t computeChecksum(CLMessage32 message);

  private:
    I2cInterface *i2c;
    uint16_t current_position;
    int16_t max_velocity;
    int16_t current_velocity;

    tf2::Transform transform;
    ros::Time stamp;
    unsigned int seq;
    std::string frame_id;
    std::string child_frame_id;
  };
}


#endif //PROJECT_HOST_COMM_LAYER_H
