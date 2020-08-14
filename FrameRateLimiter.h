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

class FrameRateLimiter : public FrameSource, Poco::Runnable
{
public:
	FrameRateLimiter(const int camera_init_int, const double frameRate);
	FrameRateLimiter(const std::string& camera_init_str, const double frameRate);
	~FrameRateLimiter();

	virtual cv::Mat GetNextFrame();

	void run();
	void stop();
private:
	Poco::SharedPtr<cv::VideoCapture> cam;
	
	int64_t frame_period_us;
	Poco::Timestamp frame_time;
	Poco::Event frame_available;
	Poco::Mutex mu_frame_queue;
	cv::Mat frame;

	Poco::Thread frame_thread;
	volatile bool want_to_stop;
};

