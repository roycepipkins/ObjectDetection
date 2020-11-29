#include "Detector.h"

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


Detector::Detector(const Poco::Util::AbstractConfiguration& config) :
	want_to_stop(false)
{
	

	Poco::Path yolo_config_path(config.getString("application.dir"));
	yolo_config_path.append("yolo-coco");
	Poco::Path yolo_weight_path(yolo_config_path);
	Poco::Path coco_names_path(yolo_config_path);
	yolo_config_path.append(config.getString("detector.config", "yolov4-leaky-416.cfg"));
	yolo_weight_path.append(config.getString("detector.weights",  "yolov4-leaky-416.weights"));
	coco_names_path.append(config.getString("detector.coco_names", "coco.names"));

	std::ifstream ifs(coco_names_path.toString().c_str());
	if (!ifs.is_open())
	{
		throw new Poco::RuntimeException("Unable to open " + coco_names_path.toString());
	}
		
	std::string line;
	while (std::getline(ifs, line))
	{
		classes.push_back(line);
	}

	std::string bkend = Poco::toLower(config.getString("detector.backend", ""));
	std::string target = Poco::toLower(config.getString("detector.target", ""));

	yolo_net = cv::dnn::readNetFromDarknet(yolo_config_path.toString(), yolo_weight_path.toString());
	if (!bkend.empty()) yolo_net.setPreferableBackend(StrToBackend(bkend));
	if (!target.empty()) yolo_net.setPreferableTarget(StrToTarget(target));
	output_layers = yolo_net.getUnconnectedOutLayersNames();

	if (config.has("detector.analysis_size"))
	{
		auto asize = config.getInt("yolo.analysis_size", 416);
		analysis_size = cv::Size(asize, asize);
	}

	use_low_priority = (StrToBackend(bkend) == cv::dnn::DNN_BACKEND_DEFAULT &&
		StrToTarget(target) == cv::dnn::DNN_TARGET_CPU);
	use_low_priority = config.getBool("detector.low_priority", use_low_priority);
}

Detector::~Detector()
{
}

int Detector::StrToTarget(const std::string& target)
{
	using namespace cv::dnn;
	if (target == "cuda") return DNN_TARGET_CUDA;
	if (target == "cuda_fp16") return DNN_TARGET_CUDA_FP16;
	if (target == "opencl") return DNN_TARGET_OPENCL;
	if (target == "opencl_fp16") return DNN_TARGET_CUDA_FP16;
	if (target == "myriad") return DNN_TARGET_MYRIAD;
	if (target == "vulkan") return DNN_TARGET_VULKAN;
	if (target == "fpga") return DNN_TARGET_FPGA;
	if (target == "cpu") return DNN_TARGET_CPU;

	return DNN_TARGET_CPU;
}



int Detector::StrToBackend(const std::string& bkend)
{
	using namespace cv::dnn;
	if (bkend == "cuda") return DNN_BACKEND_CUDA;
	if (bkend == "halide") return DNN_BACKEND_HALIDE;
	if (bkend == "intel_inference") return DNN_BACKEND_INFERENCE_ENGINE;
	if (bkend == "opencv") return DNN_BACKEND_OPENCV;
	if (bkend == "vkcom") return DNN_BACKEND_VKCOM;
	if (bkend == "default") return DNN_BACKEND_DEFAULT;
	return cv::dnn::DNN_BACKEND_DEFAULT;
}


uint64_t Detector::SubmitDetectionJob(const cv::Mat frame, const std::string src_name, const float confidence_threshold, const float nms_threshold)
{
	Poco::ScopedLock<Poco::Mutex> locker(mu_job_queue);
	uint64_t job_id = ++job_id_counter;
	DetectionJob job = { job_id, frame, src_name, confidence_threshold, nms_threshold };
	job_queue.push(job);
	ev_job_queue.set();
	return job_id;
}

std::optional<Detector::DetectionResult> Detector::GetDetectionJobIfComplete(const uint64_t job_id)
{
	Poco::ScopedLock<Poco::Mutex> locker(mu_job_output_map);
	auto it = job_output_map.find(job_id);
	if (it != job_output_map.end())
	{
		return it->second;
	}
	return std::optional<DetectionResult>();
}


void Detector::start()
{
	want_to_stop = false;
	job_thread.start(*this);
}

void Detector::run()
{
	using namespace Poco;
	if (use_low_priority) Thread::current()->setPriority(Thread::PRIO_LOW);
	while (!want_to_stop)
	{
		if (ev_job_queue.tryWait(100))
		{
			DetectionJob job = { 0, cv::Mat(), "", 0.0, 0.0 };
			{
				ScopedLock<Mutex> locker(mu_job_queue);
				job = job_queue.front();
				job_queue.pop();
			}
			Poco::Timestamp detection_timer;
			auto detections = detect(job.frame, job.src_name, job.confidence_threshold, job.nms_threshold);
			auto time_to_detect = detection_timer.elapsed();
			DetectionResult detection_result = { detections, time_to_detect };
			{
				ScopedLock<Mutex> locker(mu_job_output_map);
				job_output_map[job.job_id] = detection_result;
			}
		}
	}
}

void Detector::stop()
{
	want_to_stop = true;
	job_thread.join();
}


std::vector<Detection> Detector::detect(const cv::Mat frame, const std::string src_name, const float confidence_threshold, const float nms_threshold)
{
	using namespace std;
	using namespace cv;
	using namespace Poco;
	
	vector<Detection> detections;
	vector<vector<Mat>> network_outputs;
	vector<int> classIds;
	vector<float> confidences;
	vector<Rect> boxes;
	
	auto blob_img = dnn::blobFromImage(frame, 1.0 / 255.0, analysis_size, Scalar(), true, false);
	yolo_net.setInput(blob_img);
	yolo_net.forward(network_outputs, output_layers);
	
	for (auto output : network_outputs)
	{
		for (auto detection : output)
		{
			float* data = (float*)detection.data;
			for (int j = 0; j < detection.rows; ++j, data += detection.cols)
			{
				Mat scores = detection.row(j).colRange(5, detection.cols);
				Point classIdPoint;
				double confidence;
				minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
				if (confidence > confidence_threshold)
				{
					int centerX = (int)(data[0] * frame.cols);
					int centerY = (int)(data[1] * frame.rows);
					int width = (int)(data[2] * frame.cols);
					int height = (int)(data[3] * frame.rows);
					int left = centerX - width / 2;
					int top = centerY - height / 2;

					classIds.push_back(classIdPoint.x);
					confidences.push_back((float)confidence);
					boxes.push_back(Rect(left, top, width, height));
				}
			}
		}
	}

	map<int, vector<size_t> > class2indices;
	for (size_t i = 0; i < classIds.size(); i++)
	{
		if (confidences[i] >= confidence_threshold)
		{
			class2indices[classIds[i]].push_back(i);
		}
	}
	vector<Rect> nmsBoxes;
	vector<float> nmsConfidences;
	vector<int> nmsClassIds;
	for (map<int, vector<size_t> >::iterator it = class2indices.begin(); it != class2indices.end(); ++it)
	{
		vector<Rect> localBoxes;
		vector<float> localConfidences;
		vector<size_t> classIndices = it->second;
		for (size_t i = 0; i < classIndices.size(); i++)
		{
			localBoxes.push_back(boxes[classIndices[i]]);
			localConfidences.push_back(confidences[classIndices[i]]);
		}
		vector<int> nmsIndices;
		dnn::NMSBoxes(localBoxes, localConfidences, confidence_threshold, nms_threshold, nmsIndices);
		for (size_t i = 0; i < nmsIndices.size(); i++)
		{
			size_t idx = nmsIndices[i];
			nmsBoxes.push_back(localBoxes[idx]);
			nmsConfidences.push_back(localConfidences[idx]);
			nmsClassIds.push_back(it->first);
		}
	}
	boxes = nmsBoxes;
	classIds = nmsClassIds;
	confidences = nmsConfidences;



	for (size_t idx = 0; idx < boxes.size(); ++idx)
	{
		Detection detection;
		detection.bounding_box = boxes[idx];
		detection.confidence = confidences[idx];
		detection.detection_class = classes[classIds[idx]];
		detection.is_null = false;
		detection.frame = frame;
		detection.src_name = src_name;
		detections.push_back(detection);
	}

	if (detections.empty())
	{
		Detection null_detection;
		null_detection.src_name = src_name;
		null_detection.is_null = true;
		detections.push_back(null_detection);
	}

	return detections;
}


