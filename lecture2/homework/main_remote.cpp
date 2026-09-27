#include "io/camera.hpp"
#include "tasks/yolo.hpp"
#include "opencv2/opencv.hpp"
#include "tools/img_tools.hpp"

int main()
{
  // 你现在没有相机，我们先直接读取本地图片测试代码
  cv::Mat img = cv::imread("../../lecture1/img/1.jpg");
  
  if (img.empty()) {
    std::cerr << "无法读取 demo.jpg，请检查路径！" << std::endl;
    return -1;
  }

  // 创建 YOLO 识别器
  auto_aim::YOLO yolo("./configs/yolo.yaml");

  // 调用yolo识别装甲板
  auto armors = yolo.detect(img);

  // 画框和文字
  for (const auto & armor : armors) {
    // 1. 画绿色闭合矩形
    tools::draw_points(img, armor.points, cv::Scalar(0, 255, 0), 2);
      
    // 2. 画文字（显示颜色+数字）
    // auto_aim::COLORS 和 ARMOR_NAMES 是定义在 armor.hpp 里的字符串数组
    std::string text = auto_aim::COLORS[armor.color] + auto_aim::ARMOR_NAMES[armor.name];
      
    // 在装甲板中心点上方一点点画字
    cv::Point text_pos(armor.center.x - 30, armor.center.y - 20); 
    tools::draw_text(img, text, text_pos, cv::Scalar(0, 255, 0), 1.0, 2);
  }

  // 显示结果
  cv::imshow("img", img);
  cv::waitKey(0); // 按任意键退出
  return 0;
}