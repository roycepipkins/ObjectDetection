#pragma once
#include <opencv2/opencv.hpp>

class FrameSource
{
public:
	virtual cv::Mat GetNextFrame() = 0;
};