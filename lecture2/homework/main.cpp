#include "io/camera.hpp"
#include "tasks/yolo.hpp"
#include "opencv2/opencv.hpp"
#include "tools/img_tools.hpp"

int main()
{
  // 1. 初始化相机和 YOLO 对象
  io::Camera camera("./configs/yolo.yaml");
  auto_aim::YOLO yolo("./configs/yolo.yaml");

  cv::Mat img;
  std::chrono::steady_clock::time_point timestamp;

  // 2. 实机循环：连续读取相机画面
  while (true) {
    // 从相机读取一帧
    camera.read(img, timestamp);

    if (img.empty()) continue; // 如果没读到，跳过

    // YOLO 识别装甲板
    auto armors = yolo.detect(img);

    // 画绿色框 + 文字
    for (const auto & armor : armors) {
      // 画绿色闭合矩形
      tools::draw_points(img, armor.points, cv::Scalar(0, 255, 0), 2);
      
      // 拼出文字（比如 "bluefour"）
      std::string text = auto_aim::COLORS[armor.color] + auto_aim::ARMOR_NAMES[armor.name];
      
      // 在装甲板中心点左上方画字
      cv::Point text_pos(armor.center.x - 30, armor.center.y - 20); 
      tools::draw_text(img, text, text_pos, cv::Scalar(0, 255, 0), 2.0, 2);
    }

    // 显示画面
    cv::resize(img, img, cv::Size(1200, 800));
    cv::imshow("img", img);

    // 按 'q' 键退出循环
    if (cv::waitKey(1) == 'q') break;
  }

  return 0;
}