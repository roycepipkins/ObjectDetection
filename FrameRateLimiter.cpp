#include "FrameRateLimiter.h"

FrameRateLimiter::FrameRateLimiter(const std::string& camera_init_str, const int camera_init_int, const double frameRate):
	frame_period_us((1.0/frameRate) * 1000000.0),
	want_to_stop(false)
{
	if (camera_init_str.empty())
	{
		cam = new cv::VideoCapture(camera_init_int);
	}
	else
	{
		cam = new cv::VideoCapture(camera_init_str);
	}

	frame_thread.start(*this);
}

FrameRateLimiter::~FrameRateLimiter()
{
	stop();
}

void FrameRateLimiter::stop()
{
	want_to_stop = true;
	if (frame_thread.isRunning()) frame_thread.join();
}

cv::Mat FrameRateLimiter::GetNextFrame()
{
	if (frame_available.tryWait(frame_period_us * 2))
	{
		Poco::ScopedLock<Poco::Mutex> locker(mu_frame_queue);
		return frame;
	}
	return cv::Mat();
}

#include <opencv2/highgui.hpp>

void FrameRateLimiter::run()
{
	
	frame_time.update();
	cv::Mat fast_frame;
	Poco::Thread::sleep(1000);
	while (!want_to_stop)
	{
		*cam >> fast_frame;

		if (fast_frame.empty()) break;
		if (frame_time.elapsed() >= frame_period_us)
		{
			frame_time.update();
			Poco::ScopedLock<Poco::Mutex> locker(mu_frame_queue);
			frame = fast_frame;
			frame_available.set();
		}
	}
}
