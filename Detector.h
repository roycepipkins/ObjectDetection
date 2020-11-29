#pragma once
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <optional>
#include <cinttypes>

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>
#include <Poco/BasicEvent.h>
#include <Poco/Util/ConfigurationView.h>


#include <opencv2/dnn.hpp>

#include "Detection.h"

class Detector : public Poco::Runnable
{
public:
	Detector(const Poco::Util::AbstractConfiguration& config);
	virtual ~Detector();

	void start();
	void run();
	void stop();

	uint64_t SubmitDetectionJob(const cv::Mat frame, const std::string src_name, const float confidence_threshold, const float nms_threshold);
	std::optional<std::vector<Detection>> GetDetectionJobIfComplete(const uint64_t job_id);

private:
	volatile bool want_to_stop;

	std::vector<std::string> classes;
	
	cv::dnn::dnn4_v20200609::Net yolo_net;
	
	std::vector<cv::String> output_layers;
	cv::Size analysis_size;
	
	int StrToBackend(const std::string& tech);
	int StrToTarget(const std::string& target);

	uint64_t job_id_counter;
	std::vector<Detection> detect(const cv::Mat frame, const std::string src_name, const float confidence_threshold, const float nms_threshold);

	struct DetectionJob
	{
		uint64_t job_id;
		cv::Mat frame;
		std::string src_name;
		float confidence_threshold;
		float nms_threshold;
	};

	Poco::Mutex mu_job_queue;
	Poco::Event ev_job_queue;
	std::queue<DetectionJob> job_queue;
	Poco::Thread job_thread;
	bool use_low_priority;

	Poco::Mutex mu_job_output_map;
	std::map<uint64_t, std::vector<Detection>> job_output_map;
};

