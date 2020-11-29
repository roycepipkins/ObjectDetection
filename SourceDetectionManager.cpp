#include "SourceDetectionManager.h"

#include <Poco/Path.h>
#include <Poco/Exception.h>
#include <Poco/String.h>
#include <Poco/Debugger.h>
#include <Poco/Timestamp.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <fstream>
#include <iomanip>
#include <future>
#include <thread>
#include <chrono>


SourceDetectionManager::SourceDetectionManager(
	const std::string name,
	Poco::AutoPtr<FrameSource> frameSource,
	const bool showWindows,
	Detector& objectDetector,
	Poco::AutoPtr<Poco::Util::AbstractConfiguration> config):
	src_name(name),
	log(Poco::Logger::get(name)),
	isInteractive(showWindows),
	frame_source(frameSource),
	detector(objectDetector),
	confidence_threshold((float)config->getDouble("yolo.confidence_threshold", 0.35)),
	nms_threshold((float)config->getDouble("yolo.nms_threshold", 0.48)),
	want_to_stop(false)
	
{
	cam_fps = config->getDouble("fps", 0.25);
	if (cam_fps > 0)
		cam_detect_period_us = (int64_t)((1.0 / cam_fps) * 1000000.0);
	else
		cam_detect_period_us = 0;
	
}

SourceDetectionManager::~SourceDetectionManager()
{
	stop();
}


void SourceDetectionManager::start()
{
	log.information("Starting");
	frame_source->start();
	managementThread.start(*this);
}

void SourceDetectionManager::stop()
{
	bool should_log = !want_to_stop ||
		managementThread.isRunning();

	if (should_log) log.information("Stopping");

	want_to_stop = true;

	frame_source->stop();
	if (managementThread.isRunning()) managementThread.join();
	if (should_log) log.information("Stopped");
}




void SourceDetectionManager::drawPred(std::string class_name, float conf, int left, int top, int right, int bottom, cv::Mat& frame)
{
	using namespace cv;
	rectangle(frame, Point(left, top), Point(right, bottom), Scalar(0, 255, 0));

	std::string label = format("%.2f", conf);	
	label = class_name + ": " + label;
	

	int baseLine;
	Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

	top = max(top, labelSize.height);
	rectangle(frame, Point(left, top - labelSize.height),
		Point(left + labelSize.width, top + baseLine), Scalar::all(255), FILLED);
	putText(frame, label, Point(left, top), FONT_HERSHEY_SIMPLEX, 0.5, Scalar());
}

void SourceDetectionManager::run()
{
	if (Poco::Thread::current() == &managementThread) management();
}




//TODO refactor this to use a Detector object passed in during construction.
void SourceDetectionManager::management()
{
	
	using namespace std;
	using namespace cv;
	
	while (!want_to_stop)
	{
		try
		{
			vector<Detection> detections;

			if (isInteractive)
			{
				cv::namedWindow(src_name, cv::WINDOW_AUTOSIZE);
			}


			Poco::Timestamp detection_timer;
			ostringstream buf;
			bool is_new_detection = false;
			bool detection_in_progress = false;
			uint64_t detection_job_id = 0;
			while (!want_to_stop)
			{
				cv::Mat frame(frame_source->GetNextFrame());
				
				if (frame.empty())
				{
					log.error("Got an empty frame. Will retry.");
					Poco::Thread::sleep(1000);
					break;
				}

				if (!detection_in_progress)
				{

					//TODO in CPU mode this sucks down all the time. Since it is a stdlib thread there is no way to set priority
					//TODO I think you are screwed without priority in CPU mode. I think this is why you used the frame rate limited video source
					//There is no POCO equivalent of futures AFAIK.
					//I think you need to refactor to give the Detector is own Poco thread with a lower priority
					//Then use some sort of interface to allow mutiple clients to submit jobs and then get their own
					//results (as opposed to some other client's) back later when they are done.
					//TODO So what does that interface look like? A submit where you get back an ID for your job? Then you can poll for that ID
					//being complete or not?
					//TODO I made a couple function definitions in Detector.h. Mull those over. 
					if (detection_timer.elapsed() >= cam_detect_period_us)
					{
						detection_job_id = detector.SubmitDetectionJob(frame, src_name, confidence_threshold, nms_threshold);
						detection_timer.update();
						detection_in_progress = true;
					}
				}
				else
				{	
					optional<vector<Detection>> possible_detection = detector.GetDetectionJobIfComplete(detection_job_id);
					if (possible_detection.has_value())
					{
						detections = possible_detection.value();
						is_new_detection = true;
						detection_in_progress = false;	
					}
				}



				if (is_new_detection)
				{
					detectionEvent.notify(this, detections);
					is_new_detection = false;
				}


				if (isInteractive)
				{

					for (auto& detection : detections)
					{
						if (!detection.is_null) 
							drawPred(detection.detection_class, detection.confidence,
								detection.bounding_box.x, detection.bounding_box.y, 
								detection.bounding_box.x + detection.bounding_box.width, detection.bounding_box.y + detection.bounding_box.height, frame);
					}

					putText(frame, buf.str(), Point(10, 30), FONT_HERSHEY_PLAIN, 2.0, Scalar(0, 0, 255), 2, LINE_AA);
					cv::imshow(src_name, frame);
					waitKey(1);
				}
			}
		}
		catch (std::exception& e)
		{
			log.error(e.what());
			Poco::Thread::sleep(5000);
		}
	}
}
