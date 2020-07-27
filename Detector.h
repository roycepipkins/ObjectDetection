#pragma once
#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>
#include <Poco/BasicEvent.h>
#include <Poco/Util/ConfigurationView.h>



#include "Detection.h"
#include "FrameRateLimiter.h"

#include <opencv2/dnn.hpp>

#include <string>

class Detector : public Poco::Runnable
{
public:
	Detector(const std::string name, bool showWindows, Poco::AutoPtr<Poco::Util::AbstractConfiguration> config);
	virtual ~Detector();

	void start();
	void run();
	void stop();

	Poco::BasicEvent<std::vector<Detection>> detectionEvent;

private:
	std::string src_name;
	Poco::Logger& log;
	const std::string cam_location;
	const int cam_index;
	double cam_fps;
	int64_t cam_detect_period_us;
	Poco::SharedPtr<cv::VideoCapture> cam;
	std::vector<std::string> classes;
	bool isInteractive;
	void drawPred(std::string class_name, float conf, int left, int top, int right, int bottom, cv::Mat& frame);

	cv::dnn::dnn4_v20200609::Net yolo_net;
	//cv::dnn::dnn4_v20200310::Net yolo_net;
	std::vector<cv::String> output_layers;
	cv::Size analysis_size;
	float confidence_threshold;
	float nms_threshold;
	int StrToBackend(const std::string& tech);
	int StrToTarget(const std::string& target);

	Poco::Thread detector_thread;
	volatile bool want_to_stop;

	bool use_pre_limiter;
	Poco::SharedPtr<FrameRateLimiter> frame_rate_limiter;
	void GetNextFrame(cv::Mat& frame);
};

