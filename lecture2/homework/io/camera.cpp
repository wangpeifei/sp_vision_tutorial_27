#include "io/camera.hpp"
#include "io/hikrobot/hikrobot.hpp"
#include "tools/yaml.hpp"
#include <stdexcept>

namespace io
{

Camera::Camera(const std::string & config_path)
{
  auto yaml = tools::load(config_path);
  auto camera_name = tools::read_or<std::string>(yaml, "camera_name", "hikrobot");

  if (camera_name == "hikrobot") {
    auto exposure_ms = tools::read_or<double>(yaml, "exposure_ms", 3.0);
    auto gain = tools::read_or<double>(yaml, "gain", 10.0);
    auto vid_pid = tools::read_or<std::string>(yaml, "vid_pid", "2bdf:0001");
    camera_ = std::make_unique<HikRobot>(exposure_ms, gain, vid_pid);
  } else {
    throw std::runtime_error("Unknown camera_name: " + camera_name);
  }
}

Camera::~Camera() = default;

void Camera::read(cv::Mat & img, std::chrono::steady_clock::time_point & timestamp)
{
  camera_->read(img, timestamp);
}

}  // namespace io