#pragma once
#include <opencv2/opencv.hpp>

#include <string>
#include <vector>



class Detection
{
public:
	cv::Rect bounding_box;
	float confidence;
	std::string detection_class;
	inline int centerX() const { return bounding_box.x + bounding_box.width / 2; }
	inline int centerY() const { return bounding_box.y + bounding_box.height / 2; }
	cv::Mat frame;
	std::string src_name;
	std::string Name() { return src_name + "." + detection_class; }
	bool is_null = true;
};

