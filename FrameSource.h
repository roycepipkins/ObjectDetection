#pragma once
#include <opencv2/opencv.hpp>
#include <Poco/RefCountedObject.h>

class FrameSource : public Poco::RefCountedObject
{
public:
	virtual cv::Mat GetNextFrame(const int wait_ms = 100) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;
};