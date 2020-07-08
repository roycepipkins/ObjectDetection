#include "Detector.h"

#include <Poco/Path.h>
#include <Poco/Exception.h>
#include <Poco/String.h>
#include <Poco/Debugger.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <fstream>
#include <iomanip>


Detector::Detector(const std::string name, bool showWindows, Poco::AutoPtr<Poco::Util::AbstractConfiguration> config):
	src_name(name),
	log(Poco::Logger::get(name)),
	vidSrc(
		config->getString("location", ""),
		config->getInt("index", 0),
		config->getDouble("fps", 0.25)
	),
	isInteractive(showWindows),
	analysis_size(cv::Size(320, 320)),
	confidence_threshold((float)config->getDouble("yolo.confidence_threshold", 0.35)),
	nms_threshold((float)config->getDouble("yolo.nms_threshold", 0.3)),
	want_to_stop(false)
{
	Poco::Path yolo_config_path(config->getString("application.dir"));
	yolo_config_path.append("yolo-coco");
	Poco::Path yolo_weight_path(yolo_config_path);
	Poco::Path coco_names_path(yolo_config_path);
	yolo_config_path.append(config->getString("yolo.config", "yolov3.cfg"));
	yolo_weight_path.append(config->getString("yolo.weights",  "yolov3.weights"));
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

	yolo_net = cv::dnn::readNetFromDarknet(yolo_config_path.toString(), yolo_weight_path.toString());
	output_layers = yolo_net.getUnconnectedOutLayersNames();

	if (config->has("yolo.analysis_size"))
	{
		auto asize = config->getInt("yolo.analysis_size");
		analysis_size = cv::Size(asize, asize);
	}

	
}

Detector::~Detector()
{
	stop();
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

void Detector::drawPred(int classId, float conf, int left, int top, int right, int bottom, cv::Mat& frame)
{
	using namespace cv;
	rectangle(frame, Point(left, top), Point(right, bottom), Scalar(0, 255, 0));

	std::string label = format("%.2f", conf);
	if (!classes.empty())
	{
		CV_Assert(classId < (int)classes.size());
		label = classes[classId] + ": " + label;
	}

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
	try
	{
		vector<Detection> detections;

		if (isInteractive)
		{
			cv::namedWindow(src_name, cv::WINDOW_AUTOSIZE);
		}

		

		while (!want_to_stop)
		{
			detections.clear();

			cv::Mat frame = vidSrc.GetNextFrame();
			
			vector<vector<Mat>> outputs;
			int64 t = getTickCount(); //--- timer start -----
			auto blob_img = dnn::blobFromImage(frame, 1.0 / 255.0, Size(416, 416), Scalar(), true, false);
			yolo_net.setInput(blob_img);
			yolo_net.forward(outputs, output_layers);
			t = getTickCount() - t; //--- timer stop -----


			if (isInteractive)
			{
				ostringstream buf;
				buf << "YOLO Dectect Time: " << fixed << setprecision(3) << ((double)t / getTickFrequency()) << "s";
				putText(frame, buf.str(), Point(10, 30), FONT_HERSHEY_PLAIN, 2.0, Scalar(0, 0, 255), 2, LINE_AA);
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

				if (isInteractive)
				{
					Rect box = boxes[idx];
					drawPred(classIds[idx], confidences[idx], box.x, box.y,
						box.x + box.width, box.y + box.height, frame);
				}
			}

			if (detections.empty())
			{
				Detection null_detection;
				null_detection.src_name = src_name;
				null_detection.is_null = true;
				detections.push_back(null_detection);
			}

			detectionEvent.notify(this, detections);

			if (isInteractive)
			{
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
		log.fatal(e.what());
	}
}
