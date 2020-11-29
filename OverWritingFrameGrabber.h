#pragma once
#include <Poco/Runnable.h>
#include <Poco/Timestamp.h>
#include <Poco/Event.h>
#include <Poco/Mutex.h>
#include <Poco/SharedPtr.h>
#include <Poco/Thread.h>
#include <queue>
#include <opencv2/videoio.hpp>
#include "FrameSource.h"

class OverWritingFrameGrabber : public FrameSource, Poco::Runnable
{
public:
	OverWritingFrameGrabber(const int camera_init_int);
	OverWritingFrameGrabber(const std::string& camera_init_str);
	~OverWritingFrameGrabber();

	virtual cv::Mat GetNextFrame(const int wait_ms = 100) override;

	void start() override;
	void run() override;
	void stop() override;
private:
	Poco::SharedPtr<cv::VideoCapture> cam;
	Poco::Event frame_available;
	Poco::Mutex mu_frame;
	cv::Mat frame;

	Poco::Thread frame_thread;
	volatile bool want_to_stop;
};

