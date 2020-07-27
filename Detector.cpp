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


Detector::Detector(const std::string name, bool showWindows, Poco::AutoPtr<Poco::Util::AbstractConfiguration> config):
	src_name(name),
	log(Poco::Logger::get(name)),
	cam_location(config->getString("location", "")),
	cam_index(config->getInt("index", 0)),
	
	isInteractive(showWindows),
	analysis_size(cv::Size(416, 416)),
	confidence_threshold((float)config->getDouble("yolo.confidence_threshold", 0.35)),
	nms_threshold((float)config->getDouble("yolo.nms_threshold", 0.48)),
	want_to_stop(false),
	use_pre_limiter(true)
{
	cam_fps = config->getDouble("fps", 0.25);
	if (cam_fps > 0 && !use_pre_limiter)
		cam_detect_period_us = (int64_t)((1.0 / cam_fps) * 1000000.0);
	else
		cam_detect_period_us = 0;

	Poco::Path yolo_config_path(config->getString("application.dir"));
	yolo_config_path.append("yolo-coco");
	Poco::Path yolo_weight_path(yolo_config_path);
	Poco::Path coco_names_path(yolo_config_path);
	yolo_config_path.append(config->getString("yolo.config", "yolov4-leaky-416.cfg"));
	yolo_weight_path.append(config->getString("yolo.weights",  "yolov4-leaky-416.weights"));
	coco_names_path.append(config->getString("yolo.coco_names", "coco.names"));

	std::ifstream ifs(coco_names_path.toString().c_str());
	if (!ifs.is_open())
	{
		log.fatal(coco_names_path.toString() + " could not be opened");
		throw new Poco::RuntimeException("Unable to open " + coco_names_path.toString());
	}
		
	std::string line;
	while (std::getline(ifs, line))
	{
		classes.push_back(line);
	}

	std::string bkend = Poco::toLower(config->getString("yolo.backend", ""));
	std::string target = Poco::toLower(config->getString("yolo.target", ""));

	yolo_net = cv::dnn::readNetFromDarknet(yolo_config_path.toString(), yolo_weight_path.toString());
	if (!bkend.empty()) yolo_net.setPreferableBackend(StrToBackend(bkend));
	if (!target.empty()) yolo_net.setPreferableTarget(StrToTarget(target));
	output_layers = yolo_net.getUnconnectedOutLayersNames();

	if (config->has("yolo.analysis_size"))
	{
		auto asize = config->getInt("yolo.analysis_size", 416);
		analysis_size = cv::Size(asize, asize);
	}

	
}

Detector::~Detector()
{
	stop();
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

void Detector::GetNextFrame(cv::Mat& frame)
{
	if (frame_rate_limiter.isNull())
	{
		if (!cam.isNull()) *cam >> frame;
	}
	else
	{
		frame = frame_rate_limiter->GetNextFrame();
	}
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

void Detector::start()
{
	detector_thread.start(*this);
}

void Detector::stop()
{
	want_to_stop = true;
	if (detector_thread.isRunning()) detector_thread.join();
}

void Detector::drawPred(std::string class_name, float conf, int left, int top, int right, int bottom, cv::Mat& frame)
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

void Detector::run()
{
	using namespace std;
	using namespace cv;
	while (!want_to_stop)
	{
		try
		{
			if (!use_pre_limiter)
			{
				if (cam_location.empty())
				{
					cam = new cv::VideoCapture(cam_index);
				}
				else
				{
					cam = new cv::VideoCapture(cam_location);
				}
			}
			else
			{
				frame_rate_limiter = new FrameRateLimiter(cam_location, cam_index, cam_fps);
			}
			

			vector<Detection> detections;

			if (isInteractive)
			{
				cv::namedWindow(src_name, cv::WINDOW_AUTOSIZE);
			}


			Poco::Timestamp detection_timer;
			ostringstream buf;
			bool is_new_detection = false;
			while (!want_to_stop)
			{
				cv::Mat frame;
				GetNextFrame(frame);

				if (frame.empty())
				{
					log.error("Got an empty frame. Will retry.");
					Poco::Thread::sleep(5000);
					break;
				}

				if (detection_timer.elapsed() >= cam_detect_period_us)
				{
					detections.clear();
					is_new_detection = true;
					detection_timer.update();

					vector<vector<Mat>> outputs;
					int64 t = getTickCount(); //--- timer start -----
					auto blob_img = dnn::blobFromImage(frame, 1.0 / 255.0, Size(416, 416), Scalar(), true, false);
					yolo_net.setInput(blob_img);
					yolo_net.forward(outputs, output_layers);
					t = getTickCount() - t; //--- timer stop -----


					//TODO change this to statitics and log it every once in a while?
					if (isInteractive)
					{
						buf = ostringstream();
						buf << "YOLO Dectect Time: " << fixed << setprecision(3) << ((double)t / getTickFrequency()) << "s";

					}

					std::vector<int> classIds;
					std::vector<float> confidences;
					std::vector<Rect> boxes;


					for (auto output : outputs)
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

					std::map<int, std::vector<size_t> > class2indices;
					for (size_t i = 0; i < classIds.size(); i++)
					{
						if (confidences[i] >= confidence_threshold)
						{
							class2indices[classIds[i]].push_back(i);
						}
					}
					std::vector<Rect> nmsBoxes;
					std::vector<float> nmsConfidences;
					std::vector<int> nmsClassIds;
					for (std::map<int, std::vector<size_t> >::iterator it = class2indices.begin(); it != class2indices.end(); ++it)
					{
						std::vector<Rect> localBoxes;
						std::vector<float> localConfidences;
						std::vector<size_t> classIndices = it->second;
						for (size_t i = 0; i < classIndices.size(); i++)
						{
							localBoxes.push_back(boxes[classIndices[i]]);
							localConfidences.push_back(confidences[classIndices[i]]);
						}
						std::vector<int> nmsIndices;
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





				//TODO you need to emit these detection to another part of the system
				//TODO so they can be sent to MQTT, URL, or whatever.
				//TODO THIS is a decent spot for a Poco Event or Poco Notification
				//TODO you now need a scheme for defining event responders in your properties file

				//All the event types can be represented by a container of pairs<class id/name, geometry>


				//Types of events
				//class detection true/false
				//class instance detection count (integer)
				//class detection geometery. (bounding box + center)

				//POSSIBLE types of transports
				//**mqtt

				//**Web client
				//	GET URL (maybe no dectection info but optionally some URL vars)
				//	POST URL (event info as JSON)
				//	REST POST where I define most of the URL

				//**web server
				//	REST server where the most recent detection can be queries
				//	generic server that returns a JSON block with everything.

				//mqtt.broker
				//mqtt.user
				//mqtt.password
				//mqtt.ssl
				//mqtt.topic_prefix
				//should the body just be JSON w/ data
				//prolly one
				//but may another with just the detection count and class in topic


			}
		}
		catch (std::exception& e)
		{
			//TODO an error here kill the service. Should probably die or try to restart or something. But now it just sits there doing nothing.
			log.error(e.what());
			Poco::Thread::sleep(5000);
		}
	}
	
}
