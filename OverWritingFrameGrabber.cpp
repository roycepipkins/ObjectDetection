#include "OverWritingFrameGrabber.h"
#include <opencv2/highgui.hpp>

OverWritingFrameGrabber::OverWritingFrameGrabber(const std::string& camera_init_str):
	want_to_stop(false)
{
	if (camera_init_str.empty())
	{
		throw std::runtime_error("camera location cannot be empty");
	}
	else
	{
		cam = new cv::VideoCapture(camera_init_str);
	}
}

OverWritingFrameGrabber::OverWritingFrameGrabber(const int camera_init_int) :
	want_to_stop(false)
{
	cam = new cv::VideoCapture(camera_init_int);
}


OverWritingFrameGrabber::~OverWritingFrameGrabber()
{
	stop();
}

void OverWritingFrameGrabber::start()
{
	frame_thread.start(*this);
}

void OverWritingFrameGrabber::stop()
{
	want_to_stop = true;
	if (frame_thread.isRunning()) frame_thread.join();
}

cv::Mat OverWritingFrameGrabber::GetNextFrame(const int wait_ms)
{
	if (frame_available.tryWait(wait_ms))
	{
		Poco::ScopedLock<Poco::Mutex> locker(mu_frame);
		return frame;
	}
		
	return cv::Mat();
}

void OverWritingFrameGrabber::run()
{	
	cv::Mat local_frame;
	Poco::Thread::sleep(1000);
	while (!want_to_stop)
	{
		*cam >> local_frame;

		if (local_frame.empty()) break;
		else
		{
			Poco::ScopedLock<Poco::Mutex> locker(mu_frame);
			frame = local_frame;
			frame_available.set();
		}
	}
}
