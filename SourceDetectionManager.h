#pragma once
#include <string>
#include <vector>

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>
#include <Poco/BasicEvent.h>
#include <Poco/Util/ConfigurationView.h>

#include <opencv2/dnn.hpp>

#include "Detector.h"
#include "FrameSource.h"


class SourceDetectionManager : public Poco::Runnable, public Poco::RefCountedObject
{
public:
	SourceDetectionManager(
		const std::string name, 
		Poco::AutoPtr<FrameSource> frameSource,
		const bool showWindows, 
		Detector& objectDetector,
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> config);
	virtual ~SourceDetectionManager();

	void start();
	void run();
	void stop();

	Poco::BasicEvent<std::vector<Detection>> detectionEvent;


private:
	

	std::string src_name;
	Poco::Logger& log;

	double cam_fps;
	int64_t cam_detect_period_us;
	bool isInteractive;
	
	volatile bool want_to_stop;
	float confidence_threshold;
	float nms_threshold;
	Poco::AutoPtr<FrameSource> frame_source;
	Detector& detector;


	void drawPred(std::string class_name, float conf, int left, int top, int right, int bottom, cv::Mat& frame);

	

	void management();
	Poco::Thread managementThread;

	
};

