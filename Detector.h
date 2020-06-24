#pragma once
#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>
#include <Poco/Util/ConfigurationView.h>

#include "FrameRateLimiter.h"

#include <opencv2/dnn.hpp>

#include <string>

class Detector : public Poco::Runnable
{
public:
	Detector(const std::string name, bool showWindows, Poco::AutoPtr<Poco::Util::AbstractConfiguration> config);
	virtual ~Detector();
	void run();
	void stop();


private:
	std::string src_name;
	Poco::Logger& log;
	FrameRateLimiter vidSrc;
	std::vector<std::string> classes;
	bool isInteractive;
	void drawPred(int classId, float conf, int left, int top, int right, int bottom, cv::Mat& frame);

	cv::dnn::dnn4_v20200310::Net yolo_net;
	std::vector<cv::String> output_layers;
	cv::Size analysis_size;
	float confidence_threshold;
	float nms_threshold;

	Poco::Thread detector_thread;
	volatile bool want_to_stop;
};

